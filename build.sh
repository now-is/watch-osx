clang -o watchfile watchfile.c
clang -o watchdir watchdir.c     -framework CoreServices
clang -o watchpower watchpower.c -framework IOKit -framework CoreFoundation
clang -o watchusb watchusb.c     -framework IOKit -framework CoreFoundation 
