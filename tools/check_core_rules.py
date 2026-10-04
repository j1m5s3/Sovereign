#!/usr/bin/env python3
"""Static checks for the rules core's foundations (specs/sovereign/engine-and-architecture.md).

Fails if core/include or core/src use floating point, nondeterministic or
platform-varying facilities, or any Unreal header. Comments are ignored.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BANNED = [
    (r"\bfloat\b|\bdouble\b|\blong double\b", "floating point (use sov::Fixed)"),
    (r"<random>|\bstd::rand\b|\brand\s*\(|\bsrand\b|random_device", "nondeterministic or implementation-defined RNG (use sov::Rng)"),
    (r"\bunordered_(map|set|multimap|multiset)\b", "unordered containers (iteration order varies by platform)"),
    (r"<chrono>|\btime\s*\(|<ctime>|\bclock\s*\(", "wall-clock time in rules"),
    (r"<cmath>|<math\.h>", "floating-point math library"),
    (r'#include\s*["<](CoreMinimal|Engine/|UObject/|GameFramework/|Modules/|HAL/|Misc/|Containers/|Math/)|\.generated\.h', "Unreal headers (the core must stay engine-independent)"),
    (r"\b(UCLASS|USTRUCT|UENUM|UPROPERTY|UFUNCTION|GENERATED_BODY|IMPLEMENT_MODULE|IMPLEMENT_PRIMARY_GAME_MODULE|UE_LOG|TEXT)\s*\(", "Unreal macros (the core must stay engine-independent)"),
]


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def main():
    problems = []
    for folder in ("core/include", "core/src"):
        for path in sorted((ROOT / folder).rglob("*")):
            if path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
                continue
            code = strip_comments(path.read_text(encoding="utf-8"))
            for lineno, line in enumerate(code.splitlines(), 1):
                for pattern, why in BANNED:
                    if re.search(pattern, line):
                        problems.append(f"{path.relative_to(ROOT)}:{lineno}: {why}: {line.strip()}")
    for p in problems:
        print(p)
    if problems:
        return 1
    print("core rules check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
