import argparse
from typing import Any
from pathlib import Path

import jsonlines  # type: ignore


def compare_data(
    data1: dict[str, Any],
    data2: dict[str, Any],
    prefix: str
) -> None:
    pattern = data1["pattern"]
    strings = data1[f"{prefix}_inputs"]

    steps1 = data1[f"{prefix}_steps"]
    steps2 = data2[f"{prefix}_steps"]
    for string, step1, step2 in zip(strings, steps1, steps2):
        if step1 is None and step2 is None:
            continue
        if step1 < step2:
            print(pattern)
            print(string)
            print(step1)
            print(step2)


def compare_datasets(file1: Path, file2: Path, prefix: str) -> None:
    dataset1 = jsonlines.open(file1, mode='r')
    dataset2 = jsonlines.open(file2, mode='r')
    for data1, data2 in zip(dataset1, dataset2):
        compare_data(data1, data2, prefix)
    dataset1.close()
    dataset2.close()


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument("file1", type=Path)
    parser.add_argument("file2", type=Path)
    parser.add_argument(
        "--input-type", type=str, choices=["positive", "negative"])
    args = parser.parse_args()
    compare_datasets(args.file1, args.file2, args.input_type)