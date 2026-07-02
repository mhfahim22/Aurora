import sys
lines = []
lines.append("## Large file benchmark — stresses lexing, parsing, IR generation")
lines.append('outputln("Benchmark: large")')
lines.append("x = 0")
for i in range(500):
    j = i * 7 % 100
    lines.append(f"a{i} = sin({j} * 3.14159 / 100.0)")
    lines.append(f"b{i} = cos({j} * 2.71828 / 100.0)")
    lines.append(f"if a{i} > b{i}:")
    lines.append("    x = x + 1")
    lines.append("else:")
    lines.append("    x = x + 0")
lines.append("outputln(x)")
lines.append('outputln("Done large")')
with open("benchmarks/bench_large.aura", "w") as f:
    f.write("\n".join(lines) + "\n")
print(f"Wrote {len(lines)} lines")
