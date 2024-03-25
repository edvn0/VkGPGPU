import argparse
import os
import subprocess
from multiprocessing import Pool

def find_files(directory, extensions=(".cpp", ".hpp")):
    """Recursively finds all files in a directory with the given extensions."""
    files_to_format = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(extensions):
                files_to_format.append(os.path.join(root, file))
    return files_to_format

def format_file(file_path, dry_run=False):
    """Either runs clang-format on the file or simulates running it."""
    if dry_run:
        return f"Would format: {file_path}"
    else:
        subprocess.run(["clang-format", "-i", file_path, "--style=file"], check=True)
        return f"Formatted: {file_path}"

def process_directory(args):
    """Processes each directory: finds files and optionally formats them."""
    directory, dry_run = args
    files_to_format = find_files(directory)
    results = []
    for file_path in files_to_format:
        result = format_file(file_path, dry_run)
        results.append(result)
    return results

def main():
    parser = argparse.ArgumentParser(description="Format .cpp and .hpp files with clang-format using multiprocessing.")
    parser.add_argument("directories", nargs="+", help="List of directories to search for .cpp and .hpp files.")
    parser.add_argument("--dry-run", action="store_true", help="Only list the files that would be formatted.")
    args = parser.parse_args()

    directories = [(dir_path, args.dry_run) for dir_path in args.directories if os.path.isdir(dir_path)]

    with Pool() as pool:
        results = pool.map(process_directory, directories)

    for result in results:
        for message in result:
            print(message)

if __name__ == "__main__":
    main()
