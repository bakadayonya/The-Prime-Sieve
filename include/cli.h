// ============================================================================
//  cli.h — 命令行参数解析
// ============================================================================

#ifndef PRIME_SIEVE_CLI_H
#define PRIME_SIEVE_CLI_H

// 解析命令行；返回 false 表示应立即退出（帮助/错误）。
bool parse_arguments(int argc, char* argv[]);
void print_usage(const char* prog);

#endif // PRIME_SIEVE_CLI_H
