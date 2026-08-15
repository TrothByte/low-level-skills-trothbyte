// intentionally incorrect
// Hardcoded output path: the build script must not choose output locations,
// because that breaks caching, concurrency, and composability. Outputs are
// LazyPaths (addOutputFileArg) and installs go through the install step.
const std = @import("std");

pub fn build(b: *std.Build) void {
    const exe = b.addExecutable(.{
        .name = "hello",
        .root_module = b.createModule(.{
            .root_source_file = b.path("hello.zig"),
            .target = b.graph.host,
        }),
    });

    b.installArtifact(exe);

    const run_exe = b.addRunArtifact(exe);
    run_exe.addArg("--log-file");
    run_exe.addArg("zig-out/log.txt");
    const run_step = b.step("run", "Run with hardcoded log path");
    run_step.dependOn(&run_exe.step);
}
