// for libraries to do operations on generics
// not ergonomic for runtime code

/* trait list:
hash: T -> size_t
eq: T,T -> bool
ord: T,T -> int
add: T,T -> T
add_inplace: mut *T,T -> void // e.g. strcat
zero: void -> T // additive identity
mul: T,T -> T
mul_inplace: mut *T,T -> void
one: void -> T // multiplicative identity
*/

// returns the symbol for a derived function
ctt_str derive_symbol(const char *T, const char *trait) {
}

// derive the given traits for a primitive type
ctt_str derive(const char *T, ...) {
	// todo: metaprogram a binary search with if/elses
	if (strcmp(T, "int") == 0) {
	} else if (strcmp(T, "char*") == 0) {
	} else if (strcmp(T, "size_t") == 0) {
	} else if (strcmp(T, "float") == 0) {
	} else if (strcmp(T, "double") == 0) {
	}
}

ctt_str derive_int(...) {
	if (strcmp(arg, "add") == 0) {
		return ct_format(
			"int %s(int a, int b) { return a+b; }\n",
			derive_symbol("int", "add")
		);
	} else if (strcmp(arg, "add_inplace") == 0) {
		return ct_format(
			"void %s(int *a, int b) { *a += b; }\n",
			derive_symbol("int", "add_inplace")
		);
	} else if (strcmp(arg, "zero") == 0) {
		return ct_format(
			"int %s() { return 0; }\n",
			derive_symbol("int", "zero")
		);
	} else if (strcmp(arg, "float") == 0) {
	} else if (strcmp(arg, "double") == 0) {
	}
}

// derive the given traits for a struct
ctt_str derive_struct(const char *struct_def, ...) {
}

