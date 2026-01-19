}

This is probably caused by the lexer in the preprocessingmechanism interpreting
        the two quotes in this source file as the beginning and end of a
            string.If it does not recognize this as a string then it will
                process the file just fine and
    recognize the second #if 0 as a preprocessing directive and
    not linke now as part of a string.

        Thanks Andreas
#endif

        void
        get_mangled_name() {
}
