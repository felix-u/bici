package main

import "base:intrinsics"
import "core:os"
import "core:strings"
import "core:slice"
import "core:unicode"
import "core:strconv"
import "core:mem"
import "core:log"
import "core:fmt"

main :: proc() {
    context.logger = log.create_console_logger(opt = { .Level, .Short_File_Path, .Line, .Procedure })

    usage := "bici <compile|run|script> <file...>"
    if len(os.args) < 3 {
        log.error("usage:", usage);
        os.exit(1)
    }

    required_argument_count := 3
    command: enum { compile, run, script }
    command_string := os.args[1]
    if strings.equal_fold(command_string, "compile") {
        required_argument_count = 4
        usage = "compile <file.asm> <file.rom>"
        command = .compile
    } else if strings.equal_fold(command_string, "run") {
        usage = "run <file.rom>"
        command = .run
    } else if strings.equal_fold(command_string, "script") {
        usage = "script <file.asm>"
        command = .script
    } else {
        log.errorf("no such command '%v'\n%v", command_string, usage)
        os.exit(1)
    }
    if len(os.args) != required_argument_count {
        log.error("usage:", usage)
        os.exit(1)
    }

    input_filepath := os.args[2]
    input_bytes, _ := os.read_entire_file(input_filepath)

    rom := slice.into_dynamic((&[0x10000]u8{})[:])

    should_compile := command == .compile || command == .script
    assert(should_compile)

    if should_compile {
        Address :: distinct u16

        Token_Kind :: enum u8 {
            left_square_bracket = '[',
            right_square_bracket = ']',
            left_curly_bracket = '{',
            right_curly_bracket = '}',
            dollar = '$',
            slash = '/',
            colon = ':',
            comma = ',',
            _ = 128, // end of ASCII

            symbol,
            number,
            string,
            opmode
        }
        Token :: struct { start_index: u32, length: u8, kind: Token_Kind, value: u16, base: u8 }
        Token_Id :: struct { file_id: File_Id, index: i32 }

        Input_File :: struct { name: string, bytes: []u8, tokens: [dynamic]Token }
        File_Id :: distinct u8

        Label :: struct { token_id: Token_Id, address: Address, children: map[string]Label }
        Label_Reference :: struct {
            token_id: Token_Id,
            destination: union { Address, Token_Id },
            width: u8,
            is_patch: bool,
            parent: string,
        }

        Scoped_Reference :: struct { address: Address, token_id: Token_Id, is_relative: bool }

        Insertion_Mode :: struct { is_active, has_relative_reference: bool }

        Assembler_Context :: struct {
            files: [dynamic]Input_File,
            file_id: File_Id,
            current_global_label: string,
            global_labels: map[string]Label,
            label_references: [dynamic]Label_Reference, // TODO(felix): map
            scoped_references: [dynamic]Scoped_Reference,
            insertion_mode: Insertion_Mode,
        }
        using assembler_context : Assembler_Context
        context.user_ptr = &assembler_context

        append_elem(&files, (Input_File){ bytes = input_bytes, name = input_filepath })

        File_Cursor :: struct { token_id: Token_Id, cursor: uint }
        file_cursor_stack : [dynamic]File_Cursor
        append_elem(&file_cursor_stack, (File_Cursor){})

        parse_error :: proc(start: $T, #any_int length: int = 1, override_file_id: File_Id = 0, exit := true) where T == Token || intrinsics.type_is_numeric(T) {
            assembler_context : ^Assembler_Context = auto_cast context.user_ptr
            file_id := assembler_context.file_id
            if override_file_id != 0 do file_id = override_file_id

            when T == Token {
                start_index := start.start_index
                length := start.length
            } else {
                start_index := start
            }

            using file := assembler_context.files[file_id]
            end := start_index + auto_cast length
            lexeme := cast(string) bytes[start_index:end]

            row := 1
            column := 1
            for c in bytes[:start_index] {
                column += 1
                if c == '\n' {
                    row += 1
                    column = 1
                }
            }

            log.infof("'%v' in %v:%v:%v", lexeme, name, row, column)
            if exit do os.exit(1)
        }

        token_get_file :: proc(id: Token_Id) -> ^Input_File {
            assembler_context := cast(^Assembler_Context) context.user_ptr
            return &assembler_context.files[id.file_id]
        }

        token_get :: proc(id: Token_Id) -> ^Token {
            file := token_get_file(id)^
            if id.index >= auto_cast len(file.tokens) {
                @static nil_token : Token
                return &nil_token
            }
            token := &file.tokens[id.index]
            return token
        }

        token_lexeme :: proc(id: Token_Id) -> string {
            token := token_get(id)^
            file := token_get_file(id)
            return auto_cast file.bytes[token.start_index:token.start_index + auto_cast token.length]
        }

        switch_file: for len(file_cursor_stack) > 0 {
            file_cursor := pop(&file_cursor_stack)
            file_id = file_cursor.token_id.file_id
            file := &files[file_id]
            bytes := file.bytes

            for cursor := file_cursor.cursor; cursor < len(bytes); {
                for cursor < auto_cast len(bytes) && unicode.is_white_space(cast(rune) bytes[cursor]) do cursor += 1
                if cursor == len(bytes) do break

                start_index := cursor
                kind : Token_Kind
                value : u16
                base : u8

                is_starting_symbol_character :: proc(c: u8) -> bool { return unicode.is_alpha(cast(rune) c) || c == '_' }
                is_hexadecimal :: proc(c: u8) -> bool {
                    if '0' <= c && c <= '9' do return true
                    if 'a' <= c && c <= 'f' do return true
                    if 'A' <= c && c <= 'F' do return true
                    return false
                }

                if is_starting_symbol_character(bytes[cursor]) {
                    kind = .symbol
                    cursor += 1

                    for ; cursor < len(bytes); cursor += 1 {
                        c := bytes[cursor]
                        is_symbol_character := is_starting_symbol_character(c) || unicode.is_number(cast(rune) c)
                        if !is_symbol_character do break
                    }
                } else do switch bytes[cursor] {
                case '0':
                    kind = .number

                    if cursor + 1 == len(bytes) || bytes[cursor + 1] != 'x' && bytes[cursor + 1] != 'b' {
                        log.error("expected 'x' or 'b' after '0' to begin binary or hexadecimal literal")
                        parse_error(cursor + 1)
                    }
                    base_character := bytes[cursor + 1]
                    base = 2 if base_character == 'b' else 16
                    cursor += 2
                    start_index = cursor

                    if cursor == len(bytes) {
                        log.errorf("expected digits following '0%c'", base_character)
                        parse_error(cursor - 2, 2)
                    }

                    for ; cursor < len(bytes); cursor += 1 {
                        c := bytes[cursor]
                        if base_character == 'x' && is_hexadecimal(c) do continue
                        if base_character == 'b' && (c == '0' || c == '1') do continue
                        if unicode.is_white_space(cast(rune) c) do break
                        log.error("expected digit of base", base)
                        parse_error(cursor)
                    }

                    number_string := bytes[start_index:cursor]
                    value_unbounded, _ := strconv.parse_uint(auto_cast number_string, auto_cast base)
                    if value_unbounded > 0x10000 {
                        log.errorf("0d%v is too large to fit in 16 bits", value_unbounded)
                        parse_error(start_index, auto_cast (cursor - start_index))
                    }
                    value = auto_cast value_unbounded
                case ';':
                    for cursor < len(bytes) && bytes[cursor] != '\n' do cursor += 1
                    cursor += 1
                    continue
                case ':', '$', '{', '}', '[', ']', ',', '/':
                    kind = auto_cast bytes[cursor]
                    cursor += 1
                case '"':
                    kind = .string

                    cursor += 1
                    start_index = cursor
                    for ; cursor < len(bytes); cursor += 1 {
                        c := bytes[cursor]
                        if c == '"' do break
                        if c == '\n' {
                            log.error("expected '\"' to close string before newline")
                            parse_error(start_index, auto_cast (cursor - start_index))
                        }
                    }

                    if cursor == len(bytes) {
                        log.error("expected '\"' to close string before end of file")
                        parse_error(start_index, auto_cast (cursor - start_index))
                    }

                    assert(bytes[cursor] == '"')
                    cursor += 1
                case '.':
                    kind = .opmode

                    cursor += 1
                    start_index = cursor
                    for ; cursor < len(bytes); cursor += 1 {
                        c := bytes[cursor]
                        if unicode.is_white_space(cast(rune) c) do break
                        if c != '2' && c != 'k' && c != 'r' {
                            log.error("only characters '2', 'k', and 'r' are valid op modes")
                            parse_error(cursor)
                        }
                    }

                    length := cursor - start_index
                    if length == 0 || length > 3 {
                        log.error("a valid op mode contains 1, 2, or 3 characters (as in op.2kr), but this one has", length)
                        parse_error(start_index, auto_cast length)
                    }
                case:
                    log.errorf("invalid syntax '%c'", bytes[cursor])
                    parse_error(cursor)
                }

                length := cursor - start_index
                if kind == .string do length -= 1

                token := Token{
                    start_index = auto_cast start_index,
                    length = auto_cast length,
                    kind = kind,
                    value = value,
                    base = base,
                }
                append(&file.tokens, token)
            }

            parsing_relative_label := false
            for file_token_id := file_cursor.token_id; file_token_id.index < auto_cast len(file.tokens); file_token_id.index += 1 {
                token := token_get(file_token_id)^
                token_string := token_lexeme(file_token_id)

                if insertion_mode.is_active do #partial switch token.kind {
                case .symbol, .right_square_bracket, .dollar, .slash, .number, .string: {}
                case:
                    log.error("insertion mode only supports addresses (e.g. labels) and numeric literals")
                    parse_error(token)
                }

                token_id_shift :: proc(token_id: Token_Id, #any_int shift: int) -> Token_Id {
                    result := token_id
                    result.index += auto_cast shift
                    return result
                }

                next_id := token_id_shift(file_token_id, 1)
                next := token_get(next_id)^
                next_string := token_lexeme(next_id)

                #partial switch token.kind {
                case .left_curly_bracket:
                    append(&scoped_references, Scoped_Reference{ address = auto_cast len(rom) })
                    non_zero_resize(&rom, len(rom) + 2)
                case .right_curly_bracket:
                    if len(scoped_references) == 0 {
                        log.error("'}' has no matching '{'")
                        parse_error(token)
                    }

                    reference := pop(&scoped_references)
                    if reference.is_relative {
                        log.error("expected to resolve absolute reference (from '{') but found unresolved relative reference")
                        parse_error(token)
                    }

                    as_u16_bytes : []u8 = mem.any_to_bytes(intrinsics.byte_swap(reference.address));
                    append(&rom, ..as_u16_bytes)
                case .left_square_bracket:
                    if insertion_mode.is_active {
                        log.error("cannot enter insertion mode while already in insertion mode")
                        parse_error(token)
                    }
                    insertion_mode.is_active = true

                    compile_byte_count_to_closing_bracket := next.kind == .dollar
                    if compile_byte_count_to_closing_bracket {
                        insertion_mode.has_relative_reference = true
                        append(&scoped_references, Scoped_Reference{ address = auto_cast len(rom), is_relative = true })
                        non_zero_resize(&rom, len(rom) + 1)
                        file_token_id = token_id_shift(file_token_id, 1)
                    }
                case .right_square_bracket:
                    if !insertion_mode.is_active {
                        log.error("']' has no matching '['")
                        parse_error(token)
                    }

                    if insertion_mode.has_relative_reference {
                        reference := pop(&scoped_references)
                        if !reference.is_relative {
                            log.error("expected to resolve relative reference (from '[$') but found unresolved absolute reference; is there an unmatched '{'?")
                            parse_error(token)
                        }

                        relative_difference := len(rom) - auto_cast reference.address - 1
                        if relative_difference > 255 {
                            log.errorf("relative difference from '[$' is %v bytes, but the maximum is 255", relative_difference)
                            parse_error(token)
                        }

                        rom[reference.address] = cast(u8) relative_difference
                    }

                    insertion_mode = {}
                case .slash:
                    if next.kind != .symbol {
                        log.error("expected relative label following '/'")
                        parse_error(next)
                    }
                    parsing_relative_label = true
                case .symbol:
                    if parsing_relative_label && next.kind != .colon {
                        log.error("expected ':' ending relative label definition")
                        parse_error(next)
                    }

                    is_label := next.kind == .colon
                    if is_label {
                        if token_string in global_labels {
                            when ODIN_DEBUG {
                                for label_string in global_labels {
                                    _ = label_string
                                    label := global_labels[label_string]
                                    _ = label
                                }
                            }
                            log.errorf("redefinition of label '%v'", token_string)
                            parse_error(token, exit = false)
                            original_definition := global_labels[token_string]
                            log.info("previous definition is here:")
                            parse_error(token_get(original_definition.token_id)^)
                        }

                        new_label := Label{ token_id = file_token_id, address = auto_cast len(rom) }
                        if parsing_relative_label {
                            parent_labels := &global_labels
                            if len(current_global_label) != 0 {
                                label := &global_labels[current_global_label]
                                parent_labels = &label.children
                            }

                            if token_string in parent_labels {
                                log.errorf("redefinition of label '%v'", token_string)
                                parse_error(token)
                            }

                            parent_labels[token_string] = new_label

                            parsing_relative_label = false
                        } else {
                            current_global_label = token_string
                            global_labels[token_string] = new_label
                        }

                        file_token_id = token_id_shift(file_token_id, 1)
                        break
                    }

                    instruction : Vm_Instruction
                    is_opcode := false
                    for opcode in Vm_Opcode {
                        if strings.compare(token_string, vm_opcode_name_table[opcode]) == 0 {
                            instruction.opcode = opcode
                            is_opcode = true
                            break
                        }
                    }

                    if is_opcode {
                        explicit_mode := next.kind == .opmode
                        if explicit_mode {
                            using instruction.mode
                            for c in next_string do switch c {
                                case '2': size = .short
                                case 'k': keep = 0x80
                                case 'r': stack = .call
                                case: panic("unreachable")
                            }
                            file_token_id = token_id_shift(file_token_id, 1)
                        }

                        append(&rom, byte_from_instruction(instruction))

                        if instruction_takes_immediate[instruction.opcode] {
                            next = token_get(token_id_shift(file_token_id, 1))^
                            #partial switch next.kind {
                            case .symbol, .number, .left_curly_bracket: {}
                            case:
                                log.errorf("instruction '%v' takes an immediate, but no label or numeric literal is given", vm_opcode_name(auto_cast instruction.opcode));
                                parse_error(next)
                            }

                            if next.kind == .left_curly_bracket do continue

                            width : u8 = 1
                            if instruction.mode.size == .short || vm_opcode_is_special(instruction.opcode) do width = 2

                            if next.kind == .symbol {
                                reference := Label_Reference{
                                    token_id = token_id_shift(file_token_id, 1),
                                    destination = cast(Address) len(rom),
                                    width = width,
                                    parent = current_global_label,
                                }
                                append(&label_references, reference)

                                non_zero_resize(&rom, len(rom) + auto_cast reference.width)
                            } else {
                                assert(next.kind == .number)
                                next_string = token_lexeme(token_id_shift(file_token_id, 1))
                                value := next.value

                                if width == 1 {
                                    if value > 255 {
                                        log.errorf("attempt to supply 16-bit value to instruction taking 8-bit immediate (%v is greater than 255); did you mean to use mode '.2'?", value)
                                        parse_error(next)
                                    }
                                    append(&rom, cast(u8) value)
                                } else {
                                    assert(width == 2)
                                    as_u16_bytes : []u8 = mem.any_to_bytes(intrinsics.byte_swap(value));
                                    append(&rom, ..as_u16_bytes)
                                }
                            }
                            file_token_id = token_id_shift(file_token_id, 1)
                        }

                        break
                    }

                    if strings.compare(token_string, "org") == 0 || strings.compare(token_string, "rorg") == 0 {
                        if next.kind != .number {
                            log.errorf("expected numeric literal (byte offset) after directive '%v'", token_string)
                            parse_error(next)
                        }

                        value := next.value
                        is_relative := token_string[0] == 'r'
                        if is_relative do value += auto_cast len(rom)

                        if auto_cast value < len(rom) {
                            log.errorf("new offset %v is less than current offset %v", value, len(rom))
                            parse_error(next)
                        }

                        non_zero_resize(&rom, value)

                        file_token_id = token_id_shift(file_token_id, 1)
                        break
                    } else if strings.compare(token_string, "patch") == 0 {
                        if next.kind != .symbol {
                            log.error("expected label to indicate destination offset as first argument to directive 'patch'")
                            parse_error(next)
                        }

                        file_token_id = token_id_shift(file_token_id, 1)
                        reference := Label_Reference{
                            is_patch = true,
                            destination = file_token_id,
                            parent = current_global_label,
                        }

                        next = token_get(token_id_shift(file_token_id, 1))^
                        if next.kind != .comma {
                            log.error("expected ',' between first and second arguments to directive 'patch'")
                            parse_error(next)
                        }

                        file_token_id = token_id_shift(file_token_id, 1)
                        next = token_get(token_id_shift(file_token_id, 1))^
                        if next.kind != .symbol && next.kind != .number {
                            log.error("expected label or numeric literal to indicate address as second argument to directive 'patch'")
                            parse_error(next)
                        }

                        file_token_id = token_id_shift(file_token_id, 1)
                        reference.token_id = file_token_id
                        append(&label_references, reference)
                    } else if strings.compare(token_string, "include") == 0 {
                        if next.kind != .string {
                            log.error("expected string following directive 'include'")
                            parse_error(next)
                        }

                        include_filepath := next_string
                        if strings.compare(include_filepath, file.name) == 0 {
                            log.errorf("cyclic inclusion of file '%v'", file.name)
                            parse_error(next)
                        }
                        include_bytes, ok := os.read_entire_file(include_filepath)
                        if !ok {
                            log.errorf("unable to read file '%v'", include_filepath)
                            parse_error(next)
                        }

                        new_file := Input_File{ bytes = include_bytes, name = include_filepath }
                        new_file_id := cast(File_Id) append(&files, new_file)

                        current := File_Cursor{ cursor = len(bytes), token_id = token_id_shift(file_token_id, 2) }
                        append(&file_cursor_stack, current)

                        next_file := File_Cursor{ token_id = { file_id = new_file_id } }
                        append(&file_cursor_stack, next_file)
                        continue switch_file
                    }

                    reference := Label_Reference{
                        token_id = file_token_id,
                        destination = cast(Address) len(rom),
                        width = 2,
                        parent = current_global_label,
                    }
                    append(&label_references, reference)
                    non_zero_resize(&rom, len(rom) + auto_cast reference.width)
                case .number:
                    if !insertion_mode.is_active {
                        log.error("standalone number literals are only supported in insertion mode (e.g. in [ ... ])")
                        parse_error(token)
                    }

                    value := token.value

                    is_byte := (token.base == 16 && len(token_string) <= 2) || (token.base == 2 && len(token_string) <= 8)
                    if is_byte {
                        assert(value < 256)
                        append(&rom, cast(u8) value)
                        break
                    }

                    is_short := true
                    if is_short {
                        as_u16_bytes : []u8 = mem.any_to_bytes(intrinsics.byte_swap(value));
                        append(&rom, ..as_u16_bytes)
                        break
                    }

                    panic("unreachable")
                case .string:
                    if !insertion_mode.is_active {
                        log.error("strings can only be used in insertion mode (e.g. inside [ ... ])")
                        parse_error(token)
                    }

                    append(&rom, token_string)
                case:
                    log.error("invalid syntax")
                    parse_error(token)
                }
            }

            if len(scoped_references) != 0 {
                reference := scoped_references[0]
                token := token_get(reference.token_id)
                log.error("anonymous reference ('{') without matching '}'")
                parse_error(token^, override_file_id = reference.token_id.file_id)
            }
        }

        for reference in label_references {
            source_label, destination_label : ^Label
            to_match := [?]^^Label{ &source_label, &destination_label }
            match_count := 2 if reference.is_patch else 1
            match_against := [2]Token_Id{ reference.token_id, {} }
            if match_count == 2 do match_against[1] = reference.destination.(Token_Id)

            for match, i in to_match[:match_count] {
                token_id := match_against[i]
                token := token_get(token_id)
                reference_string := token_lexeme(token_id)

                if token.kind == .number {
                    @static dummy : Label
                    dummy.address = auto_cast token.value
                    source_label = &dummy
                    continue
                }
                assert(token.kind == .symbol)

                resolving: {
                    parent := global_labels[reference.parent]

                    if reference_string in parent.children {
                        match^ = &parent.children[reference_string]
                        break resolving
                    }

                    if reference_string in global_labels {
                        match^ = &global_labels[reference_string]
                        break resolving
                    }
                }
                if match^ == nil {
                    when ODIN_DEBUG {
                        parent := global_labels[reference.parent]
                        for label_string in parent.children {
                            _ = label_string
                            label := parent.children[label_string]
                            _ = label
                        }
                    }
                    log.errorf("no such label '%v'", reference_string)
                    parse_error(token^, override_file_id = token_id.file_id)
                }
            }

            if reference.is_patch {
                as_u16_bytes : []u8 = mem.any_to_bytes(intrinsics.byte_swap(source_label.address))
                copy(rom[destination_label.address:][:2], as_u16_bytes)
            } else do switch reference.width {
            case 1:
                rom[reference.destination.(Address)] = cast(u8) source_label.address
            case 2:
                as_u16_bytes : []u8 = mem.any_to_bytes(intrinsics.byte_swap(source_label.address))
                copy(rom[reference.destination.(Address):][:2], as_u16_bytes)
            case: panic("unreachable")
            }
        }

        if command == .compile {
            output_path := os.args[3]
            ok := os.write_entire_file(output_path, rom[:], truncate = false)
            assert(ok)
        }
    }

    should_run := command == .script || command == .run
    if should_run {
        vm : Vm
        copy(vm.memory[:], rom[:])

        print_memory := true
        if print_memory {
            fmt.println("MEMORY ===")
            for token_id in 0x100..<clamp(len(rom), 0, 0x200) {
                byte := vm.memory[token_id]

                mode_string := ""
                if !vm_opcode_is_special(auto_cast byte) do switch (byte & 0xe0) >> 5 {
                case 0x1: mode_string = "2"
                case 0x2: mode_string = "r"
                case 0x3: mode_string = "r2"
                case 0x4: mode_string = "k"
                case 0x5: mode_string = "k2"
                case 0x6: mode_string = "kr"
                case 0x7: mode_string = "kr2"
                }

                fmt.printfln("[%03x]\t'%c'\t#%02x\t%v;%v", token_id, byte, byte, vm_opcode_name(byte), mode_string)
            }
            fmt.println("\nRUN ===")
        }

        unimplemented()
    }
}

Vm :: struct {
    memory: [0x10000]u8,
    stacks: [Vm_Stack_Id]Vm_Stack,
    active_stack: Vm_Stack_Id,
    program_counter: u16,
    current_mode: Vm_Instruction_Mode,
    // TODO(felix): render context

    file_bytes: ^u8,
}

Vm_Stack :: struct { memory: [0x100]u8, data: u8 }

Vm_Opcode :: enum u8 {
    break_                      = 0x00, /* NO MODE */
    push                        = 0x01, /* NO k MODE */
    drop                        = 0x02,
    nip                         = 0x03,
    swap                        = 0x04,
    rotate                      = 0x05,
    duplicate                   = 0x06,
    over                        = 0x07,
    equals                      = 0x08,
    not_equals                  = 0x09,
    greater_than                = 0x0a,
    less_than                   = 0x0b,
    add                         = 0x0c,
    subtract                    = 0x0d,
    multiply                    = 0x0e,
    divide                      = 0x0f,
    increment                   = 0x10,
    not                         = 0x11,
    and                         = 0x12,
    or                          = 0x13,
    xor                         = 0x14,
    shift                       = 0x15,
    jump                        = 0x16,
    jump_if_equal               = 0x17,
    jump_stash_return           = 0x18,
    jump_not_equal              = 0x19,
    jump_not_equal_immediate    = 0x1a,
    stash                       = 0x1b,
    load                        = 0x1c,
    store                       = 0x1d,
    read                        = 0x1e,
    write                       = 0x1f,
    jump_immediate              = 0x80, /* NO MODE (== push;k)  */
    jump_if_equal_immediate     = 0xa0, /* NO MODE (== push;k2) */
    jump_stash_return_immediate = 0xc0, /* NO MODE (== push;kr) */
    /* UNUSED      0xe0, 0)   NO MODE (== push;kr2)*/\
}

vm_opcode_name :: proc(instruction: u8) -> string {
    return vm_opcode_name_table[auto_cast (instruction if vm_opcode_is_special(auto_cast instruction) else instruction & 0b00011111)];
}

vm_opcode_is_special :: proc(instruction: Vm_Opcode) -> bool {
    #partial switch instruction {
    case .jump_immediate, .jump_if_equal_immediate, .jump_not_equal_immediate, .jump_stash_return_immediate, .break_: return true
    }
    return false
}

vm_opcode_name_table := #sparse [Vm_Opcode]string{
    .break_                      = "break",
    .push                        = "push",
    .drop                        = "drop",
    .nip                         = "nip",
    .swap                        = "swap",
    .rotate                      = "rot",
    .duplicate                   = "dup",
    .over                        = "over",
    .equals                      = "eq",
    .not_equals                  = "neq",
    .greater_than                = "gt",
    .less_than                   = "lt",
    .add                         = "add",
    .subtract                    = "sub",
    .multiply                    = "mul",
    .divide                      = "div",
    .increment                   = "inc",
    .not                         = "not",
    .and                         = "and",
    .or                          = "or",
    .xor                         = "xor",
    .shift                       = "shift",
    .jump                        = "jmp",
    .jump_if_equal               = "jme",
    .jump_stash_return           = "jst",
    .jump_not_equal              = "jne",
    .jump_not_equal_immediate    = "jni",
    .stash                       = "stash",
    .load                        = "load",
    .store                       = "store",
    .read                        = "read",
    .write                       = "write",
    .jump_immediate              = "jmi",
    .jump_if_equal_immediate     = "jei",
    .jump_stash_return_immediate = "jsi",
}

Vm_Stack_Id :: enum u8 { working, call }
Vm_Opcode_Size :: enum u8 { byte, short }
Vm_Instruction_Mode :: struct { keep: u8, stack: Vm_Stack_Id, size: Vm_Opcode_Size }
Vm_Instruction :: struct { opcode: Vm_Opcode, mode: Vm_Instruction_Mode }

byte_from_instruction :: proc(using instruction: Vm_Instruction) -> u8 {
    if vm_opcode_is_special(opcode) do return auto_cast opcode
    using mode
    byte : u8 = transmute(u8) opcode | (keep << 7) | (cast(u8) stack << 6) | (cast(u8) size << 5)
    return byte
}

instruction_takes_immediate := #partial #sparse [Vm_Opcode]bool{
    .push                        = true,
    .jump_not_equal_immediate    = true,
    .jump_immediate              = true,
    .jump_if_equal_immediate     = true,
    .jump_stash_return_immediate = true,
}
