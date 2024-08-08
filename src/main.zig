const std = @import("std");

const Op = enum(u5) {
    push,
    drop,
    nip,
    swap,
    rot,
    dup,
    over,
    @"=",
    @"!=",
    @">",
    @"<",
    @"+",
    @"-",
    @"*",
    @"/",
    @"++",
    not,
    @"and",
    @"or",
    xor,
    shift,
    jmp,
    // jmi, // push;k
    jeq,
    // jei, // push;k2
    jst,
    // jsi, // push;kr2
    stash,
    load,
    loadr,
    store,
    storer,
    read,
    write,
};

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const argv = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, argv);

    const usage = "usage: bici <com|run|script> <file...>";
    if (argv.len < 3) {
        std.log.err(usage, .{});
        return error.Usage;
    }

    if (std.mem.eql(u8, argv[1], "com")) {
        if (argv.len != 4) {
            std.log.err("usage: bici com <file.biciasm> <file.bici>", .{});
            return error.Usage;
        }
        try std.fs.cwd().writeFile(.{ .sub_path = argv[3], .data = try compile(allocator, argv[2]) });
    } else if (std.mem.eql(u8, argv[1], "run")) {
        try run(try std.fs.cwd().readFileAlloc(allocator, argv[2], max_asm_filesize));
    } else if (std.mem.eql(u8, argv[1], "script")) {
        if (argv.len != 3) {
            std.log.err("usage: bici script <file.biciasm>", .{});
            return error.Usage;
        }
        try run(try compile(allocator, argv[2]));
    } else {
        std.log.err("no such command '{s}'\n{s}", .{ argv[1], usage });
        return error.InvalidCommand;
    }
}

const max_asm_filesize = 8 * 1024 * 1024;

var out = [_]u8{0} ** (0x10000 - 0x100);
var pc: u16 = 0;

var forward_res = [_]u16{0} ** 0x100;
var forward_res_idx: u8 = 0;

fn compile(allocator: std.mem.Allocator, path_asm_file: []const u8) ![]const u8 {
    const assembly = try std.fs.cwd().readFileAlloc(allocator, path_asm_file, max_asm_filesize);
    var i: usize = 0;
    var add: usize = 1;
    while (i < assembly.len) : (i += add) {
        add = 1;
        if (std.ascii.isWhitespace(assembly[i])) continue;

        var mode_k: u8 = 0;
        var mode_r: u8 = 0;
        var mode_2: u8 = 0;

        switch (assembly[i]) {
            '#' => {
                i += 1;
                if (i == assembly.len) {
                    std.log.err("expected hex digit, found EOF at '{s}'[{d}]", .{ path_asm_file, i });
                    return error.InvalidSyntax;
                }

                const beg_i = i;
                if (!std.ascii.isHex(assembly[i])) {
                    std.log.err(
                        "expected hex digit, found byte '{c}'/{d}/#{x} at '{s}'[{d}]",
                        .{ assembly[i], assembly[i], assembly[i], path_asm_file, i },
                    );
                    return error.InvalidSyntax;
                }

                while (i < assembly.len and std.ascii.isHex(assembly[i])) {
                    i += 1;
                }
                const end_i = i;
                switch (end_i - beg_i) {
                    2 => {
                        try compileOp(0, 0, 0, @intFromEnum(Op.push));
                        try write(try std.fmt.parseUnsigned(u8, assembly[beg_i..end_i], 16));
                    },
                    4 => {
                        try compileOp(0, 0, 1, @intFromEnum(Op.push));
                        try write2(try std.fmt.parseUnsigned(u16, assembly[beg_i..end_i], 16));
                    },
                    else => {
                        std.log.err(
                            "expected 2 digits (byte) or 4 digits (short), found {d} digits in number at '{s}'[{d}..{d}]",
                            .{ end_i - beg_i, path_asm_file, beg_i - 1, end_i },
                        );
                        return error.InvalidNumber;
                    },
                }
                continue;
            },
            '{' => {
                forward_res[forward_res_idx] = pc;
                forward_res_idx += 1;
                pc += 1;
                add = 2; // TODO: handle more forward resolution modes than just '#', which we're skipping over here
                continue;
            },
            '}' => {
                // TODO: more forward resolution modes
                i += 1;
                forward_res_idx -= 1;
                const res_pc = forward_res[forward_res_idx];
                out[res_pc] = @intCast(pc - res_pc);
                continue;
            },
            '"' => {
                i += 1;
                if (i == assembly.len) {
                    std.log.err("expected string, found EOF at '{s}'[{d}]", .{ path_asm_file, i });
                    return error.InvalidSyntax;
                }

                while (i < assembly.len and !std.ascii.isWhitespace(assembly[i])) : (i += 1) {
                    try write(assembly[i]);
                }
                add = 0;
                continue;
            },
            else => {},
        }

        const beg_i = i;
        i += 1;
        var end_i = i;

        while (i < assembly.len and !std.ascii.isWhitespace(assembly[i])) : ({
            i += 1;
            end_i = i;
        }) {
            if (assembly[i] != ';') continue;

            i += 1;
            while (i < assembly.len and !std.ascii.isWhitespace(assembly[i])) : (i += 1) switch (assembly[i]) {
                'k' => mode_k = 1,
                'r' => mode_r = 1,
                '2' => mode_2 = 1,
                else => {
                    std.log.err(
                        "expected one of ['k', 'r', '2'], found byte '{c}'/{d}/#{x} at '{s}'[{d}]",
                        .{ assembly[i], assembly[i], assembly[i], path_asm_file, i },
                    );
                    return error.InvalidSyntax;
                },
            };

            break;
        }

        const lexeme = assembly[beg_i..end_i];
        inline for (std.meta.tags(Op)) |tag| {
            if (std.mem.eql(u8, lexeme, @tagName(tag))) {
                try compileOp(mode_k, mode_r, mode_2, @intFromEnum(tag));
                break;
            }
        } else {
            std.log.err("invalid token '{s}' at '{s}'[{d}..{d}]", .{ lexeme, path_asm_file, beg_i, end_i });
            return error.InvalidSyntax;
        }

        continue;
    }

    return out[0..pc];
}

fn compileOp(mode_k: u8, mode_r: u8, mode_2: u8, op: u8) !void {
    try write(op | (mode_2 << 5) | (mode_r << 6) | (mode_k << 7));
}

fn write(byte: u8) !void {
    out[pc] = byte;
    pc = std.math.add(u16, pc, 1) catch |e| {
        std.log.err("exceeded maximum program size", .{});
        return e;
    };
}

fn write2(short: u16) !void {
    try write(@truncate((short & 0xff00) >> 8));
    try write(@truncate(short));
}

var memory = [_]u8{0} ** 0x10000;
var ps = [_]u8{0} ** 0x100;
var psp: u8 = 0;
var rs = [_]u8{0} ** 0x100;
var rsp: u8 = 0;
var s = &ps;
var sp = &psp;

fn sRet() void {
    s = &rs;
    sp = &rsp;
}

fn sParam() void {
    s = &ps;
    sp = &psp;
}

fn run(bytecode: []const u8) !void {
    std.debug.print("BYTECODE ===\n", .{});
    for (bytecode, 0..) |byte, i| {
        std.debug.print("[{d:2}]\t'{c}'\t#{x:02}\t{d}\n", .{ i, byte, byte, byte });
    }
    std.debug.print("\n", .{});
    // std.debug.print("BYTECODE: {x:02}\n", .{bytecode});
    defer std.debug.print("\nRUN ===\nparam:\t{x:02}\nreturn:\t{x:02}\n", .{ ps[0..psp], rs[0..rsp] });

    var i: u16 = 0;
    while (i < bytecode.len) : (i += 1) {
        // TODO jmi, jei, jsi
        // switch (bytecode[i]) {
        // }

        const op: Op = @enumFromInt(bytecode[i]);
        const mode_k = bytecode[i] >> 7;
        const mode_r = bytecode[i] >> 6;
        const mode_2 = bytecode[i] >> 5;

        if (mode_r == 1) sRet() else sParam();

        if (mode_2 == 1) switch (op) {
            .push => {
                i +%= 1;
                push2(s, sp, u16LittleEndianAt(bytecode, i));
                i +%= 1;
            },
            .@"+" => push2(s, sp, pop2(s, sp, mode_k) + pop2(s, sp, mode_k)),
            else => {
                std.log.err("todo: {}", .{op});
                return error.Todo2;
            },
        } else switch (op) {
            .push => {
                i +%= 1;
                push(s, sp, bytecode[i]);
            },
            .@"+" => push(s, sp, pop(s, sp, mode_k) + pop(s, sp, mode_k)),
            else => {
                std.log.err("todo: {}", .{op});
                return error.Todo;
            },
        }
    }
}

fn u16LittleEndianAt(bytes: []const u8, idx: anytype) u16 {
    return @as(u16, bytes[idx]) | @as(u16, bytes[idx +% 1]) << 8;
}

fn pop(stack: *[0x100]u8, stack_ptr: *u8, keep: u8) u8 {
    const popped = stack[stack_ptr.* -% 1];
    if (keep == 0) stack_ptr.* -%= 1;
    return popped;
}

fn pop2(stack: *[0x100]u8, stack_ptr: *u8, keep: u8) u16 {
    const popped = u16LittleEndianAt(stack, stack_ptr.* -% 2);
    if (keep == 0) stack_ptr.* -%= 2;
    return popped;
}

fn push(stack: *[0x100]u8, stack_ptr: *u8, value: u8) void {
    stack[stack_ptr.*] = value;
    stack_ptr.* +%= 1;
}

fn push2(stack: *[0x100]u8, stack_ptr: *u8, value: u16) void {
    push(stack, stack_ptr, @truncate(value >> 8));
    push(stack, stack_ptr, @truncate(value));
}
