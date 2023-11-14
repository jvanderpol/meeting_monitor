#!/usr/bin/env python3

import os
import re
import sys

if len(sys.argv) < 2:
  print(str(sys.argv), file=sys.stderr)
  print(f'Usage {sys.argv[0]} input_svg [input_svg...]', file=sys.stderr)
  exit(1)

lines = []
for svg_path in sys.argv[1:]:
  with open(svg_path, "r") as svg:
    name = re.sub(r"\.", "_", os.path.basename(svg_path)).upper()
    newlines_removed = re.sub(r"\n *", "", svg.read())
    whitespace_removed = re.sub(r"(>)\s*(<)" ,r"\1\2", newlines_removed)
    escaped_quotes = re.sub(r"\"" ,r"\"", whitespace_removed)
    lines.append(f'#define {name} "{escaped_quotes}"')

print("\n".join(lines))
