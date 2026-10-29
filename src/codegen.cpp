#include "codegen.h"
#include <algorithm>
#include <numeric>

namespace robotlang::extension {
namespace {
struct Block {
    int label;
    int next;
    int body;
    int slot;
    int kind;
    const Command* command;
};

bool hasAction(const std::vector<Command>& commands) {
    for (const auto& command : commands)
        if (command.kind != Command::Kind::Repeat || hasAction(command.body)) return true;
    return false;
}

int append(const std::vector<Command>& commands, int next, std::vector<Block>& blocks, int& labels, int& slots) {
    for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
        int label = labels++;
        if (it->kind == Command::Kind::Repeat) {
            if (!hasAction(it->body)) {
                blocks.push_back({label, next, next, 0, 1, &*it});
                next = label;
                continue;
            }
            int slot = slots++;
            int end = labels++;
            int body = append(it->body, end, blocks, labels, slots);
            blocks.push_back({end, next, body, slot, 2, &*it});
            blocks.push_back({label, next, body, slot, 1, &*it});
        } else blocks.push_back({label, next, 0, 0, 0, &*it});
        next = label;
    }
    return next;
}

const char* name(Command::Kind kind) {
    switch (kind) {
        case Command::Kind::Forward: return "forward";
        case Command::Kind::Left: return "left";
        case Command::Kind::Right: return "right";
        case Command::Kind::Repeat: return "repeat";
    }
    return "";
}

void skippedComments(std::ostream& out, const std::string& robot, const std::vector<Command>& commands) {
    for (const auto& command : commands) {
        out << "  # " << robot << " command #" << command.id << " @" << command.source.line << ':' << command.source.col << ' ' << name(command.kind) << ' ' << command.argument.value << '\n';
        skippedComments(out, robot, command.body);
    }
}

int facing(Direction direction) {
    switch (direction) {
        case Direction::North: return 0;
        case Direction::East: return 1;
        case Direction::South: return 2;
        case Direction::West: return 3;
    }
    return 0;
}

template<class F> void array(std::ostream& out, const char* label, std::size_t count, F value) {
    out << label << ":\n";
    if (!count) out << "  .quad 0\n";
    for (std::size_t i = 0; i < count; ++i) out << "  .quad " << value(i) << '\n';
}
}

Codegen::Codegen(std::ostream& out, Integer tickLimit) : out_(out), tickLimit_(tickLimit) {}

void Codegen::emit(const Program& program, const std::vector<Rewrite>& rewrites, bool optimized) {
    auto& o = out_;
    const std::size_t n = program.robots.size();
    std::vector<std::vector<Block>> blocks(n);
    std::vector<int> entries(n), ends(n);
    int labels = 0, slots = 0;
    for (std::size_t i = 0; i < n; ++i) {
        ends[i] = labels++;
        entries[i] = append(program.robots[i].commands, ends[i], blocks[i], labels, slots);
    }
    o << "# RobotLang " << (optimized ? "optimized" : "unoptimized") << " assembly\n";
    for (const auto& rewrite : rewrites) {
        o << "# rewrite " << rewrite.rule << ": ";
        for (std::size_t i = 0; i < rewrite.inputs.size(); ++i) {
            if (i) o << " + ";
            o << '#' << rewrite.inputs[i];
        }
        o << " -> ";
        if (rewrite.output) o << '#' << *rewrite.output;
        else o << '-';
        o << '\n';
    }
    o << ".section .rodata\n.Lfmt: .string \"%s: (%ld, %ld) %s\\n\"\n.Lfallen: .string \"%s: fallen at (%ld, %ld)\\n\"\n.Llimit: .string \"limite de ticks excedido\"\n";
    o << ".Lnorth: .string \"north\"\n.Least: .string \"east\"\n.Lsouth: .string \"south\"\n.Lwest: .string \"west\"\n";
    for (std::size_t i = 0; i < n; ++i) o << ".Lid" << i << ": .string \"" << program.robots[i].id << "\"\n";
    o << ".data\n";
    array(o, ".Lx", n, [&](std::size_t i) { return program.robots[i].x.value; });
    array(o, ".Ly", n, [&](std::size_t i) { return program.robots[i].y.value; });
    array(o, ".Ldir", n, [&](std::size_t i) { return facing(program.robots[i].facing); });
    array(o, ".Lstatus", n, [&](std::size_t i) { return program.robots[i].commands.empty() ? 1 : 0; });
    o << ".Lpc:\n";
    for (int entry : entries) o << "  .quad .Lblock" << entry << '\n';
    o << ".Lids:\n";
    for (std::size_t i = 0; i < n; ++i) o << "  .quad .Lid" << i << '\n';
    o << ".Lnames:\n  .quad .Lnorth, .Least, .Lsouth, .Lwest\n.Lsteps:\n";
    for (std::size_t i = 0; i < n; ++i) o << "  .quad .Lstep" << i << '\n';
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return program.robots[a].priority < program.robots[b].priority; });
    array(o, ".Lorder", n, [&](std::size_t i) { return order[i]; });
    array(o, ".Lwx", program.walls.size(), [&](std::size_t i) { return program.walls[i].x.value; });
    array(o, ".Lwy", program.walls.size(), [&](std::size_t i) { return program.walls[i].y.value; });
    array(o, ".Lww", program.walls.size(), [&](std::size_t i) { return program.walls[i].width.value; });
    array(o, ".Lwh", program.walls.size(), [&](std::size_t i) { return program.walls[i].height.value; });
    array(o, ".Lhx", program.holes.size(), [&](std::size_t i) { return program.holes[i].x.value; });
    array(o, ".Lhy", program.holes.size(), [&](std::size_t i) { return program.holes[i].y.value; });
    o << ".Lreps:\n  .zero " << std::max(1, slots) * 8 << "\n.text\n.globl main\n.type main, @function\nmain:\n"
      << "  pushq %rbp\n  movq %rsp, %rbp\n  pushq %r12\n  pushq %r13\n  pushq %r14\n  pushq %r15\n  xorq %r12, %r12\n"
      << ".Ltick:\n  xorq %r13, %r13\n  xorq %r14, %r14\n.Lactive:\n"
      << "  cmpq $" << n << ", %r14\n  jae .Lchecked\n  leaq .Lstatus(%rip), %rax\n  cmpq $0, (%rax,%r14,8)\n  jne .Lactive_next\n  movq $1, %r13\n.Lactive_next:\n  incq %r14\n  jmp .Lactive\n"
      << ".Lchecked:\n  testq %r13, %r13\n  jz .Lreport\n  movabsq $" << tickLimit_ << ", %rax\n  cmpq %rax, %r12\n  jae .Lerror\n  incq %r12\n  xorq %r14, %r14\n.Lschedule:\n"
      << "  cmpq $" << n << ", %r14\n  jae .Ltick\n  leaq .Lorder(%rip), %rax\n  movq (%rax,%r14,8), %r15\n  leaq .Lstatus(%rip), %rax\n  cmpq $0, (%rax,%r15,8)\n  jne .Lschedule_next\n  leaq .Lsteps(%rip), %rax\n  call *(%rax,%r15,8)\n.Lschedule_next:\n  incq %r14\n  jmp .Lschedule\n"
      << ".Lreport:\n  xorq %r14, %r14\n.Lreport_loop:\n  cmpq $" << n << ", %r14\n  jae .Lsuccess\n  leaq .Lids(%rip), %rax\n  movq (%rax,%r14,8), %rsi\n  leaq .Lx(%rip), %rax\n  movq (%rax,%r14,8), %rdx\n  leaq .Ly(%rip), %rax\n  movq (%rax,%r14,8), %rcx\n  leaq .Lstatus(%rip), %rax\n  cmpq $2, (%rax,%r14,8)\n  je .Lreport_fallen\n  leaq .Ldir(%rip), %rax\n  movq (%rax,%r14,8), %r8\n  leaq .Lnames(%rip), %rax\n  movq (%rax,%r8,8), %r8\n  leaq .Lfmt(%rip), %rdi\n  jmp .Lreport_call\n.Lreport_fallen:\n  leaq .Lfallen(%rip), %rdi\n.Lreport_call:\n  xorl %eax, %eax\n  call printf@PLT\n  incq %r14\n  jmp .Lreport_loop\n.Lsuccess:\n  xorl %eax, %eax\n  jmp .Lexit\n.Lerror:\n  leaq .Llimit(%rip), %rdi\n  call puts@PLT\n  movl $1, %eax\n.Lexit:\n  popq %r15\n  popq %r14\n  popq %r13\n  popq %r12\n  popq %rbp\n  ret\n";
    for (std::size_t i = 0; i < n; ++i) {
        o << ".Lstep" << i << ":\n  jmp *.Lpc+" << i * 8 << "(%rip)\n.Lblock" << ends[i] << ":\n  movq $1, .Lstatus+" << i * 8 << "(%rip)\n  ret\n";
        for (const auto& b : blocks[i]) {
            const auto& c = *b.command;
            o << ".Lblock" << b.label << ":\n";
            if (b.kind != 2) o << "  # " << program.robots[i].id << " command #" << c.id << " @" << c.source.line << ':' << c.source.col << ' ' << name(c.kind) << ' ' << c.argument.value << '\n';
            if (b.kind == 1) {
                if (!hasAction(c.body)) {
                    skippedComments(o, program.robots[i].id, c.body);
                    o << "  jmp .Lblock" << b.next << '\n';
                }
                else o << "  movabsq $" << c.argument.value << ", %rax\n  movq %rax, .Lreps+" << b.slot * 8 << "(%rip)\n  jmp .Lblock" << b.body << '\n';
            } else if (b.kind == 2) {
                o << "  decq .Lreps+" << b.slot * 8 << "(%rip)\n  jnz .Lblock" << b.body << "\n  jmp .Lblock" << b.next << '\n';
            } else {
                o << "  leaq .Lblock" << b.next << "(%rip), %rax\n  movq %rax, .Lpc+" << i * 8 << "(%rip)\n";
                if (c.kind == Command::Kind::Forward) {
                    o << "  movq $" << i << ", %rdi\n  movabsq $" << c.argument.value << ", %rsi\n  subq $8, %rsp\n  call .Lforward\n  addq $8, %rsp\n";
                } else {
                    Integer delta = c.argument.value / 90 * (c.kind == Command::Kind::Left ? -1 : 1);
                    o << "  addq $" << delta << ", .Ldir+" << i * 8 << "(%rip)\n  andq $3, .Ldir+" << i * 8 << "(%rip)\n";
                }
                o << "  ret\n";
            }
        }
    }
    o << ".Lforward:\n  movq %rdi, %r8\n  movq %rsi, %r9\n.Lfstep:\n  testq %r9, %r9\n  jz .Lfend\n  leaq .Lx(%rip), %rax\n  movq (%rax,%r8,8), %r10\n  leaq .Ly(%rip), %rax\n  movq (%rax,%r8,8), %r11\n  leaq .Ldir(%rip), %rax\n  movq (%rax,%r8,8), %rdx\n  cmpq $0, %rdx\n  jne .Lfeast\n  incq %r11\n  jmp .Lfbound\n.Lfeast:\n  cmpq $1, %rdx\n  jne .Lfsouth\n  incq %r10\n  jmp .Lfbound\n.Lfsouth:\n  cmpq $2, %rdx\n  jne .Lfwest\n  decq %r11\n  jmp .Lfbound\n.Lfwest:\n  decq %r10\n.Lfbound:\n  testq %r10, %r10\n  js .Lfend\n  testq %r11, %r11\n  js .Lfend\n"
      << "  movabsq $" << program.width.value << ", %rdx\n  cmpq %rdx, %r10\n  jge .Lfend\n  movabsq $" << program.height.value << ", %rdx\n  cmpq %rdx, %r11\n  jge .Lfend\n  xorq %rcx, %rcx\n.Lfwalls:\n  cmpq $" << program.walls.size() << ", %rcx\n  jae .Lfrobots_start\n"
      << "  leaq .Lwx(%rip), %rax\n  movq (%rax,%rcx,8), %rdx\n  cmpq %rdx, %r10\n  jl .Lfwall_next\n  leaq .Lww(%rip), %rax\n  addq (%rax,%rcx,8), %rdx\n  cmpq %rdx, %r10\n  jge .Lfwall_next\n  leaq .Lwy(%rip), %rax\n  movq (%rax,%rcx,8), %rdx\n  cmpq %rdx, %r11\n  jl .Lfwall_next\n  leaq .Lwh(%rip), %rax\n  addq (%rax,%rcx,8), %rdx\n  cmpq %rdx, %r11\n  jl .Lfend\n.Lfwall_next:\n  incq %rcx\n  jmp .Lfwalls\n.Lfrobots_start:\n  xorq %rcx, %rcx\n.Lfrobots:\n  cmpq $" << n << ", %rcx\n  jae .Lfmove\n  cmpq %r8, %rcx\n  je .Lfrobot_next\n  leaq .Lstatus(%rip), %rax\n  cmpq $2, (%rax,%rcx,8)\n  je .Lfrobot_next\n  leaq .Lx(%rip), %rax\n  cmpq %r10, (%rax,%rcx,8)\n  jne .Lfrobot_next\n  leaq .Ly(%rip), %rax\n  cmpq %r11, (%rax,%rcx,8)\n  je .Lfend\n.Lfrobot_next:\n  incq %rcx\n  jmp .Lfrobots\n.Lfmove:\n  leaq .Lx(%rip), %rax\n  movq %r10, (%rax,%r8,8)\n  leaq .Ly(%rip), %rax\n  movq %r11, (%rax,%r8,8)\n  xorq %rcx, %rcx\n.Lfholes:\n  cmpq $" << program.holes.size() << ", %rcx\n  jae .Lfcontinue\n  leaq .Lhx(%rip), %rax\n  cmpq %r10, (%rax,%rcx,8)\n  jne .Lfhole_next\n  leaq .Lhy(%rip), %rax\n  cmpq %r11, (%rax,%rcx,8)\n  je .Lffallen\n.Lfhole_next:\n  incq %rcx\n  jmp .Lfholes\n.Lffallen:\n  leaq .Lstatus(%rip), %rax\n  movq $2, (%rax,%r8,8)\n  ret\n.Lfcontinue:\n  decq %r9\n  jmp .Lfstep\n.Lfend:\n  ret\n.section .note.GNU-stack,\"\",@progbits\n";
}
}
