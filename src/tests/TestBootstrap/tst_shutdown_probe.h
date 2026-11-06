#pragma once

// Entry point for the exit-path subprocess probes.
//
// The probes are the only way to test "the process must end within a budget while
// something inside it is stuck", so they run as real child processes and the test
// case watches from the outside. Returns a negative value when the command line is
// not a probe request, which lets main() fall through to the regular test run.
int liteRunShutdownProbe(int argc, char *argv[]);
