namespace stub_test
{
    const void* function_a(); // function with no args
    void function_b(int param_a, int* pointer, const int& ref); // function with args
    
    class my_class
    {
    public:
        int var; // comments
        int function_c(int param); // member function
        const void* function_d() = 0; // test for virtual function
        
        int test = int(1); // initialiser
    private:
    
        int function_const() const; // const member function
        void (*function_pointer)(int b, int c); // test for function pointer
        
        inline void inline_func()
        {
            int a = (int)0.0f; // some code to avoid
        }
    };
    
    namespace deeper
    {
        void function_e(int a, float b, test c);
        int function_f(int b = 0); // test for default args
        
        class scope_class // ignore this
        {
            // parenthesis might span multiple lines
            int function_g
            (
                int a,
                int b,
                int c = 0
            );
        };
    }
    
    /*
        int commented_out_function_a(int b = 0);
        
        struct commented_out
        {
            int a;
            int b;
            int c;
        };
        
    */ void function_h(int b); // testing code on the same line as end comment
    
    // int commented_out_function_b();
}