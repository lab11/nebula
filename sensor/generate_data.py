import random
import sys

def generate_data_file(B, N):
    with open('data.h', 'w') as file:
        file.write("#ifndef DATA_H\n")
        file.write("#define DATA_H\n\n")
        file.write("#include <stdint.h>\n\n")
        file.write(f"const uint8_t data[{N}][{B}] = {{\n")

        for i in range(N):
            file.write("    {")
            for j in range(B):
                file.write(f"{random.randint(0, 255)}")
                if j < B - 1:
                    file.write(", ")
            if i < N - 1:
                file.write("},\n")
            else:
                file.write("}\n")

        file.write("};\n\n")
        file.write("#endif // DATA_H\n")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_data.py <size_in_bytes_of_each_sample> <number_of_samples>")
        sys.exit(1)

    B = int(sys.argv[1])
    N = int(sys.argv[2])

    generate_data_file(B, N)
    print("data.h file generated successfully.")

