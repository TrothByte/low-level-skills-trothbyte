const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addLibrary(.{
        .name = "fizzbuzz",
        .linkage = .static,
        .root_module = b.createModule(.{
            .root_source_file = b.path("fizzbuzz.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    const exe = b.addExecutable(.{
        .name = "demo",
        .root_module = b.createModule(.{
            .root_source_file = b.path("demo.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    exe.root_module.addCSourceFile(.{ .file = b.path("extra.c"), .flags = &.{"-std=c11"} });
    exe.root_module.linkLibrary(lib);
    exe.root_module.linkSystemLibrary("z", .{});

    b.installArtifact(lib);
    b.installArtifact(exe);
}
