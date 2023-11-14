#!/usr/bin/env python3

import re
import sys

if len(sys.argv) != 2:
  print(f'Usage {sys.argv[0]} input_css', file=sys.stderr)
  exit(1)

with open(sys.argv[1], "r") as css:
  newlines_removed = re.sub(r"\n *", "", css.read())
  whitespace_removed = re.sub(r": " ,r":", newlines_removed)
  whitespace_removed2 = re.sub(r" {" ,r"{", whitespace_removed)
  extra_semis_removed = re.sub(r";}" ,r"}", whitespace_removed2)
  escaped_quotes = re.sub(r"\"" ,r"\"", extra_semis_removed)
  print(f'#define CSS "{escaped_quotes}"')
