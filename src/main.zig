const Asm = @import("Asm.zig");
const Bici = @import("Bici.zig");
const std = @import("std");

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
        try std.fs.cwd().writeFile(.{ .sub_path = argv[3], .data = try Asm.compile(allocator, argv[2]) });
    } else if (std.mem.eql(u8, argv[1], "run")) {
        try Bici.run(try std.fs.cwd().readFileAlloc(allocator, argv[2], Asm.max_filesize));
    } else if (std.mem.eql(u8, argv[1], "script")) {
        if (argv.len != 3) {
            std.log.err("usage: bici script <file.biciasm>", .{});
            return error.Usage;
        }
        try Bici.run(try Asm.compile(allocator, argv[2]));
    } else {
        std.log.err("no such command '{s}'\n{s}", .{ argv[1], usage });
        return error.InvalidCommand;
    }
}
