import argparse
import os

def get_file_prefix(input_path, output_path, num_lines=None, num_bytes=None):
    if not os.path.exists(input_path):
        print(f"Error: The file '{input_path}' does not exist.")
        return

    try:
        # If splitting by bytes, open in binary mode
        if num_bytes is not None:
            with open(input_path, 'rb') as f:
                chunk = f.read(num_bytes)
            
            if output_path:
                with open(output_path, 'wb') as f_out:
                    f_out.write(chunk)
                print(f"Successfully wrote first {num_bytes} bytes to {output_path}")
            else:
                # Try to decode for console printing, fallback to raw bytes representation
                try:
                    print(chunk.decode('utf-8'))
                except UnicodeDecodeError:
                    print(chunk)

        # Default/If splitting by lines, open in text mode
        else:
            lines = []
            with open(input_path, 'r', encoding='utf-8', errors='ignore') as f:
                for _ in range(num_lines if num_lines is not None else 10):
                    line = f.readline()
                    if not line:
                        break
                    lines.append(line)
            
            if output_path:
                with open(output_path, 'w', encoding='utf-8') as f_out:
                    f_out.writelines(lines)
                print(f"Successfully wrote the prefix lines to {output_path}")
            else:
                print("".join(lines), end="")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Extract the prefix (start) of a file.")
    parser.add_name_or_flags = parser.add_argument
    
    parser.add_argument("input_file", help="Path to the source file.")
    parser.add_argument("-o", "--output", help="Path to save the prefix file. If omitted, prints to terminal.")
    
    # Mutually exclusive group: choose lines OR bytes
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-n", "--lines", type=int, default=10, help="Number of lines to grab (default: 10).")
    group.add_argument("-c", "--bytes", type=int, help="Number of bytes to grab.")

    args = parser.parse_args()

    get_file_prefix(
        input_path=args.input_file, 
        output_path=args.output, 
        num_lines=args.lines if args.bytes is None else None, 
        num_bytes=args.bytes
    )