#include <cstdint>
#include <fstream>
#include <chrono>
#include <random>
#include <cstring>

const unsigned int START_ADDRESS = 0x200;
const unsigned int FONTSET_SIZE = 0x80;
const unsigned int FONTSET_START_ADDRESS = 0x50;

uint8_t fontset[FONTSET_SIZE] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


class Chip8 {
public:
    uint8_t registers[16]{};
    uint8_t memory[4096]{};
    uint16_t index{};
    uint16_t pc{};
    uint16_t stack[16]{};
    uint8_t sp{};
    uint8_t delayTimer{};
    uint8_t soundTimer{};
    uint8_t keypad[16]{};
    uint32_t video[64 * 32]{};
    uint16_t opcode;

    Chip8::Chip8() : randGen(std::chrono::system_clock::now().time_since_epoch().count())
    {
        pc = START_ADDRESS;

        for (unsigned int i = 0; i < FONTSET_SIZE; ++i)
        {
            memory[FONTSET_START_ADDRESS + i] = fontset[i];
        }

        // Initialize RNG
		randByte = std::uniform_int_distribution<uint8_t>(0, 255U);
    };

    void Chip8::LoadROM(char const* filename)
    {
        // use ate in order to get file size
        std::ifstream file(filename, std::ios::binary | std::ios::ate);

        if (file.is_open())
        {
            std::streampos size = file.tellg();
            char* buffer = new char[size];

            file.seekg(0, std::ios::beg);
            file.read(buffer, size);
            file.close();

            for (unsigned int i = 0; i < size; ++i)
            {
                memory[START_ADDRESS + i] = buffer[i];
            }
        }
    }

    // CLS: clear the display
    void Chip8::OP_00E0()
    {
        memset(video, 0, sizeof(video));
    }

    // RET: return from subroutine
    void Chip8::OP_00EE()
    {
        --sp;
        pc = stack[sp];
    }

    // JP addr: jump to location
    void Chip8::OP_1nnn()
    {
        // only take last 12 bits, first 4 are the JP command
        uint16_t address = opcode & 0x0FFFu;
        pc = address;
    }

    // CALL addr: call subroutine
    void Chip8::OP_2nnn()
    {
        uint16_t address = opcode & 0x0FFFu;
        stack[sp++] = pc;
        pc = address;
    }

    // SE Vx, byte: skip next instruction if Vx = kk
    void Chip8::OP_3xkk()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;
        if (registers[Vx] == byte)
        {
            pc += 2;
        }
    }

    // SE Vx, byte: skip next instruction if Vx != kk
    void Chip8::OP_4xkk()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;
        if (registers[Vx] != byte)
        {
            pc += 2;
        }
    }

    // SE Vx, Vy: skip next instruction if Vx = Vy.
    void Chip8::OP_5xy0()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = opcode & 0x00F0u >> 4u;
        if (registers[Vx] == registers[Vy])
        {
            pc += 2;
        }
    }

    // LD Vx, byte: set Vx to byte
    void Chip8::OP_6xkk()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;
        registers[Vx] = byte;
    }

    // ADD Vx, byte: set Vx = Vx + kk
    void Chip8::OP_7xkk()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;
        registers[Vx] += byte;
    }

    // LD Vx, Vy: set Vx = Vy
    void Chip8::8xy0()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = opcode & 0x00F0u >> 4u;
        registers[Vx] = registers[Vy];
    }

    // OR Vx, Vy: set Vx = Vx OR Vy
    void Chip8::8xy1()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = opcode & 0x00F0u >> 4u;
        registers[Vx] |= registers[Vy];
    }

    // AND Vx, Vy: set Vx = Vx AND Vy
    void Chip8::8xy2()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = opcode & 0x00F0u >> 4u;
        registers[Vx] &= registers[Vy];
    }

    // XOR Vx, Vy: set Vx = Vx XOR Vy
    void Chip8::8xy3()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = opcode & 0x00F0u >> 4u;
        registers[Vx] ^= registers[Vy];
    }

    // ADD Vx, Vy: set Vx = Vx ADD Vy, Vy = carry
    void Chip8::8xy4()
    {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = opcode & 0x00F0u >> 4u;
        uint16_t sum = Vx + Vy;

        registers[0xFu] = sum > 255u ? 1 : 0;
        registers[Vx] = sum & 0xFFu;
    }

    

private:
    std::default_random_engine randGen;
    std::uniform_int_distribution<uint8_t> randByte;
};