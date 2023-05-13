bison -d -o lm_bison.cc parser.y
flex  --wincompat --header-file=lm_flex.hh -o lm_flex.cc scanner.l