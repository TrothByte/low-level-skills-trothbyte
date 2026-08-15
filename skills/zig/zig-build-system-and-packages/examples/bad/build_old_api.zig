// intentionally incorrect
// 0.14-era artifact API. Since 0.15, addExecutable takes .root_module created with
// b.createModule; the direct .root_source_file field no longer exists.
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "hello",
        .root_source_file = b.path("hello.zig"),
        .target = target,
        .optimize = optimize,
    });

    b.installArtifact(exe);
}
