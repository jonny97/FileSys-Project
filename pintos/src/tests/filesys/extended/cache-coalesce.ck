# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(cache-coalesce) begin
(cache-coalesce) created test file
(cache-coalesce) device writes should be on order of ~128
(cache-coalesce) end
EOF
pass;
