// intentionally incorrect
// Default target is the host. This build pins every artifact to the host,
// making "cross-compilation" impossible for users of -Dtarget.
const std = @import("std");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "host_only",
        .root_module = b.createModule(.{
            .root_source_file = b.path("main.zig"),
            .target = b.graph.host,
            .optimize = optimize,
        }),
    });
    b.installArtifact(exe);
}
