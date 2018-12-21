# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(cache-hitrate) begin
(cache-hitrate) created test file
(cache-hitrate) write random bytes
(cache-hitrate) read all bytes
(cache-hitrate) read all bytes again
(cache-hitrate) Second read should have greater hit rate!
(cache-hitrate) end
EOF
pass;
