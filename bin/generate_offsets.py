#!/usr/bin/env python3

import datetime
from zoneinfo import ZoneInfo

central_tz = ZoneInfo('America/Chicago')
dt_start = datetime.datetime(
    datetime.datetime.now().year,
    1, 1, 2, tzinfo=central_tz)
delta = datetime.timedelta()
one_hour = datetime.timedelta(hours=1)
thiry_years = datetime.timedelta(days=365*30)

print('int getOffset(int epoch) {')
last_offset = central_tz.utcoffset(dt_start)
while delta < thiry_years:
  delta = delta + one_hour
  checked_time = dt_start + delta
  offset = central_tz.utcoffset(checked_time)
  if last_offset != offset:
    print(f'  if (epoch < {int(checked_time.timestamp())})')
    print(f'    return {last_offset.total_seconds()};')
    last_offset = offset
print(f'  return {last_offset.total_seconds()};')
print('}')
