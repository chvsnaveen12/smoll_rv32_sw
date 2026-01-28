import sys
import os
import struct

def pad_to_block(data, block_size=512):
    padding = (block_size - (len(data) % block_size)) % block_size
    return data + b'\x00' * padding

def main():
    if len(sys.argv) < 5 or (len(sys.argv) - 3) % 2 != 0:
        print("Usage: python3 packer.py <device> <jump_addr> <file1> <addr1> [<file2> <addr2> ...]")
        sys.exit(1)

    device_path = sys.argv[1]
    jump_addr_str = sys.argv[2]
    file_args = sys.argv[3:]
    
    try:
        jump_addr = int(jump_addr_str, 0)
    except ValueError:
        print(f"Error: Invalid jump address '{jump_addr_str}'")
        sys.exit(1)
    
    entries = []
    current_lba = 1 # Data starts at Block 1
    
    files_data = []

    num_transfers = len(file_args) // 2
    
    print(f"Device: {device_path}")
    print(f"Jump Address: 0x{jump_addr:08X}")
    print(f"Number of transfers: {num_transfers}")

    for i in range(0, len(file_args), 2):
        filepath = file_args[i]
        addr_str = file_args[i+1]
        
        try:
            addr = int(addr_str, 0)
        except ValueError:
            print(f"Error: Invalid address '{addr_str}'")
            sys.exit(1)
            
        if not os.path.exists(filepath):
            print(f"Error: File '{filepath}' not found")
            sys.exit(1)
            
        with open(filepath, 'rb') as f:
            content = f.read()
            
        padded_content = pad_to_block(content)
        num_blocks = len(padded_content) // 512
        
        # If file is empty, we still might want to account for it or skip it.
        if num_blocks == 0:
            num_blocks = 1
            padded_content = b'\x00' * 512
        
        start_lba = current_lba
        end_lba = start_lba + num_blocks - 1 
        
        entries.append({
            'start': start_lba,
            'end': end_lba,
            'addr': addr
        })
        
        files_data.append(padded_content)
        current_lba += num_blocks
        
        print(f"File: {filepath}, Addr: 0x{addr:08X}, LBA: {start_lba}-{end_lba}, Size: {len(content)} bytes")

    # Construct Header (Block 0)
    # Format: Jump Addr (4), Num transfers (4), (Start, End, Addr) * N
    # All Little Endian (<)
    header = struct.pack('<II', jump_addr, num_transfers)
    for entry in entries:
        header += struct.pack('<III', entry['start'], entry['end'], entry['addr'])
        
    # Pad header to 512 bytes
    if len(header) > 512:
        print("Error: Header exceeds 512 bytes!")
        sys.exit(1)
        
    header = pad_to_block(header)
    
    # Write to device
    try:
        print(f"Writing to {device_path}...")
        with open(device_path, 'wb') as dev:
            # Write Header
            dev.write(header)
            
            # Write Files
            for data in files_data:
                dev.write(data)
                
            print("Write complete.")
            
    except PermissionError:
        print(f"Error: Permission denied writing to '{device_path}'. Try running with sudo.")
        sys.exit(1)
    except Exception as e:
        print(f"Error writing to device: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
