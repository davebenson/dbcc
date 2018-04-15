//
// So far, there have only been "leap seconds" meaning the addition of 
// extra seconds (the original specification had a plan for handling years
// that were too short, but it's never come up).
//
// Research (and common sense) indicates that these things 
// are slowing down, so i suspect we'll need more leap seconds.
//
// Nonetheless, there was a gap (and Bush II briefly considered ditching
// the leap-second entirely.
//
struct DskLeapSecondTable
{
  unsigned n_leap_second_days;
  unsigned *leap_second_days;
};

extern const DskLeapSecondTable *dsk_leap_second_current_table;


uint64_t dsk_leap_second_table_convert_unixtime_real_to_posix 
                                         (const DskLeapSecondTable *table,
                                          uint64_t real_unixtime, 
                                          uint64_t *orig_seconds_out);
uint64_t dsk_leap_second_table_convert_unixtime_posix_to_real 
                                         (const DskLeapSecondTable *table,
                                          uint64_t posix_unixtime);
