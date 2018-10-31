namespace test
{
    class my_class
    {
        int var;
        int funct(int param);
        
        int pooper; // comments
        
        int test = int(1);
    };
    
    namespace scope2
    {
        void free_funct(int a, float b, test c);
        
        class scope_class // ignore this
        {
            int val;
            float bal;
            
            int function_punction(int b = 0);
            
            const void* tester() = 0; // test for virtual function
            
            int func
            (
                int parenthesis,
                int might_be_on_mutliple_lines
            );
        }
    }
    
    /*
        comments
    */
}