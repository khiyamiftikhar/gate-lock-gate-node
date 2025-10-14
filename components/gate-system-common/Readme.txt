This registry is type agnostic. It does not need to include the header files
It also does not store void pointers because not type safe
So it uses fwd decleration. The fwd decleration should use the exact format as done in standard header files
Otherwise it will result in error.