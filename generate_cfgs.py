import argparse
import random
from pathlib import Path

def generate_proposals(num_proposals, max_proposals_per_line, max_distinct_values):
    values = list(range(1, max_distinct_values + 1))
    proposals = []

    for _ in range(num_proposals):
        proposal_size = random.randint(1, max_proposals_per_line)
        proposal = random.sample(values, proposal_size)
        proposals.append(" ".join(map(str, proposal)))

    return proposals


def generate_config_files(output_dir, num_files, num_proposals, max_proposals_per_line, max_distinct_values):
    Path(output_dir).mkdir(parents=True, exist_ok=True)

    for i in range(1, num_files + 1):
        file_path = Path(output_dir) / f"lattice-config{i}.config"

        proposals = generate_proposals(num_proposals, max_proposals_per_line, max_distinct_values)

        with open(file_path, 'w') as f:
            f.write(f"{num_proposals} {max_proposals_per_line} {max_distinct_values}\n")
            f.write("\n".join(proposals) + "\n")

        print(f"Generated {file_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate Lattice Config Files.")
    parser.add_argument("--output_dir", required=True, help="Output directory for config files.")
    parser.add_argument("--num_files", type=int, required=True, help="Number of config files to generate.")
    parser.add_argument("--num_proposals", type=int, required=True, help="Number of proposals per config file.")
    parser.add_argument("--max_proposals_per_line", type=int, required=True, help="Max proposals per line.")
    parser.add_argument("--max_distinct_values", type=int, required=True, help="Max distinct values across all proposals.")

    args = parser.parse_args()

    generate_config_files(
        args.output_dir,
        args.num_files,
        args.num_proposals,
        args.max_proposals_per_line,
        args.max_distinct_values
    )
