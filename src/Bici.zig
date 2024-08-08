const std = @import("std");

pub const Op = enum(u5) {
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
    jeq,
    jst,
    stash,
    load,
    loadr,
    store,
    storer,
    read,
    write,
};

pub const Instruction = packed struct {
    op: Op,
    mode: packed struct {
        size: enum(u1) { byte, short } = .byte,
        stack: enum(u1) { param, ret } = .param,
        keep: bool = false,
    } = .{},

    pub const special = struct {
        pub const jmi: u8 = @bitCast(Instruction{ .op = .push, .mode = .{ .keep = true } });
        pub const jei: u8 = @bitCast(Instruction{ .op = .push, .mode = .{ .keep = true, .size = .short } });
        pub const jsi: u8 = @bitCast(Instruction{ .op = .push, .mode = .{ .keep = true, .stack = .ret } });
        pub const @"break": u8 = @bitCast(Instruction{ .op = .push, .mode = .{ .keep = true, .stack = .ret, .size = .short } });
    };
};

var stacks: struct {
    param: [0x100]u8 = [_]u8{0} ** 0x100,
    param_ptr: u8 = 0,
    ret: [0x100]u8 = [_]u8{0} ** 0x100,
    ret_ptr: u8 = 0,

    pub fn setParam(self: *@This()) void {
        stack = &self.param;
        stack_ptr = &self.param_ptr;
    }

    pub fn setRet(self: *@This()) void {
        stack = &self.ret;
        stack_ptr = &self.ret_ptr;
    }
} = .{};

var stack = &stacks.param;
var stack_ptr = &stacks.param_ptr;

var keep = false;

var memory = [_]u8{0} ** 0x10000;

pub fn run(bytecode: []const u8) !void {
    @memcpy(memory[0..bytecode.len], bytecode);

    std.debug.print("MEMORY ===\n", .{});
    for (memory[0x100..bytecode.len], 0x100..) |byte, i| std.debug.print(
        "[{x:4}]\t'{c}'\t#{x:02}\t{s}\n",
        .{ i, byte, byte, @tagName(@as(Op, @enumFromInt(byte & 0x1f))) },
    );
    std.debug.print("\nRUN ===\n", .{});
    defer std.debug.print(
        "\nSTACKS ===\nparam:\t{x:02}\nreturn:\t{x:02}\n",
        .{ stacks.param[0..stacks.param_ptr], stacks.ret[0..stacks.ret_ptr] },
    );

    var i: u16 = 0x100;
    while (i < 0x10000) : (i += 1) {
        switch (memory[i]) {
            Instruction.special.jmi => return error.Todo,
            Instruction.special.jei => return error.Todo,
            Instruction.special.jsi => return error.Todo,
            Instruction.special.@"break" => break,
            else => {},
        }

        const instruction: Instruction = @bitCast(memory[i]);

        switch (instruction.mode.stack) {
            .param => stacks.setParam(),
            .ret => stacks.setRet(),
        }
        keep = instruction.mode.keep;

        switch (instruction.mode.size) {
            .byte => switch (instruction.op) {
                .push => {
                    i +%= 1;
                    push(memory[i]);
                },
                .@"+" => push(pop() + pop()),
                .@"/" => {
                    const right = pop();
                    const left = pop();
                    push(left / right);
                },
                .write => {
                    // TODO: device table in memory[0x00..0x100] (special-cased for now)
                    std.debug.assert(pop() == 0x00);
                    i +%= 1;
                    const strlen = memory[i];
                    std.debug.print("{s}", .{memory[i + 1 ..][0..strlen]});
                    i +%= strlen;
                },
                else => {
                    std.log.err("todo: {any}", .{instruction});
                    return error.Todo;
                },
            },
            .short => switch (instruction.op) {
                .push => {
                    i +%= 1;
                    push2(u16LittleEndianAt(&memory, i));
                    i +%= 1;
                },
                .@"+" => push2(pop2() + pop2()),
                else => {
                    std.log.err("todo: {any}", .{instruction});
                    return error.Todo2;
                },
            },
        }
    }
}

fn u16LittleEndianAt(bytes: []const u8, idx: anytype) u16 {
    return @as(u16, bytes[idx]) | @as(u16, bytes[idx +% 1]) << 8;
}

fn pop() u8 {
    const popped = stack[stack_ptr.* -% 1];
    if (!keep) stack_ptr.* -%= 1;
    return popped;
}

fn pop2() u16 {
    const popped = u16LittleEndianAt(stack, stack_ptr.* -% 2);
    if (!keep) stack_ptr.* -%= 2;
    return popped;
}

fn push(value: u8) void {
    stack[stack_ptr.*] = value;
    stack_ptr.* +%= 1;
}

fn push2(value: u16) void {
    push(@truncate(value >> 8));
    push(@truncate(value));
}
