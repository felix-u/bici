const Bici = @import("Bici.zig");
const std = @import("std");

pub const max_filesize = 8 * 1024 * 1024;

var out = [_]u8{0} ** 0x10000;
var pc: u16 = 0;

var forward_res = [_]u16{0} ** 0x100;
var forward_res_idx: u8 = 0;

var path_asm_file: []const u8 = undefined;
var assembly: []const u8 = undefined;
var i: usize = 0;

pub fn compile(allocator: std.mem.Allocator, _path_asm_file: []const u8) ![]const u8 {
    path_asm_file = _path_asm_file;
    assembly = try std.fs.cwd().readFileAlloc(allocator, path_asm_file, max_filesize);
    var add: usize = 1;
    while (i < assembly.len) : (i += add) {
        add = 1;
        if (std.ascii.isWhitespace(assembly[i])) continue;

        var mode_k: u8 = 0;
        var mode_r: u8 = 0;
        var mode_2: u8 = 0;

        switch (assembly[i]) {
            '|' => {
                i += 1;
                const hex = try parseHex();
                pc = switch (hex.num_digits) {
                    1, 2 => hex.result.int8,
                    3, 4 => hex.result.int16,
                    else => unreachable,
                };
                continue;
            },
            '#' => {
                i += 1;
                const hex = try parseHex();
                switch (hex.num_digits) {
                    2 => {
                        try compileOp(0, 0, 0, @intFromEnum(Bici.Op.push));
                        try write(hex.result.int8);
                    },
                    4 => {
                        try compileOp(0, 0, 1, @intFromEnum(Bici.Op.push));
                        try write2(hex.result.int16);
                    },
                    else => unreachable,
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
                out[res_pc] = @intCast(pc - 1 - res_pc);
                continue;
            },
            '_' => {
                i += 1;
                const hex = try parseHex();
                switch (hex.num_digits) {
                    2 => try write(hex.result.int8),
                    4 => try write2(hex.result.int16),
                    else => unreachable,
                }
                continue;
            },
            '\'' => {
                i += 1;
                if (i == assembly.len) {
                    std.log.err("expected byte, found EOF at '{s}'[{d}]", .{ path_asm_file, i });
                    return error.InvalidSyntax;
                }

                try write(assembly[i]);
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
        for (std.meta.tags(Bici.Op)) |tag| {
            if (std.mem.eql(u8, lexeme, @tagName(tag))) {
                try compileOp(mode_k, mode_r, mode_2, @intFromEnum(tag));
                break;
            }
        } else inline for (@typeInfo(Bici.Instruction.special).Struct.decls) |op| {
            if (std.mem.eql(u8, lexeme, op.name)) {
                try write(@field(Bici.Instruction.special, op.name));
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

fn parseHex() !struct { num_digits: u3, result: union { int8: u8, int16: u16 } } {
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

    const num_digits = end_i - beg_i;
    return switch (num_digits) {
        1, 2 => .{
            .num_digits = @intCast(num_digits),
            .result = .{ .int8 = try std.fmt.parseUnsigned(u8, assembly[beg_i..end_i], 16) },
        },
        3, 4 => .{
            .num_digits = @intCast(num_digits),
            .result = .{ .int16 = try std.fmt.parseUnsigned(u16, assembly[beg_i..end_i], 16) },
        },
        else => {
            std.log.err(
                "expected 1-2 digits (byte) or 3-4 digits (short), found {d} digits in number at '{s}'[{d}..{d}]",
                .{ end_i - beg_i, path_asm_file, beg_i - 1, end_i },
            );
            return error.InvalidNumber;
        },
    };
}
