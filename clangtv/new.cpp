#include <iostream>

#include <clang-c/Index.h>


int main( int argc, char** argv )
{
  std::cout << "main" << std::endl;

  if( argc < 2 )
  {
    std::cout << "args" << std::endl;
    return -1;
  }

  CXIndex index        = clang_createIndex( 0, 1 );
  CXTranslationUnit tu = clang_createTranslationUnitFromSourceFile( index, argv[1], 0, nullptr, 0, nullptr );

  if( !tu )
  {
    std::cout << "tu for:" << argv[1] << std::endl;
    return -1;
  }

  CXCursor rootCursor  = clang_getTranslationUnitCursor( tu );

  clang_disposeTranslationUnit( tu );
  clang_disposeIndex( index );
  return 0;
}
lorro@Lorands-Mac-mini wg21_lorros_clang_uneval_proposal_implementation % 
lorro@Lorands-Mac-mini wg21_lorros_clang_uneval_proposal_implementation % 
lorro@Lorands-Mac-mini wg21_lorros_clang_uneval_proposal_implementation % 
lorro@Lorands-Mac-mini wg21_lorros_clang_uneval_proposal_implementation % 
lorro@Lorands-Mac-mini wg21_lorros_clang_uneval_proposal_implementation % 
lorro@Lorands-Mac-mini wg21_lorros_clang_uneval_proposal_implementation % 
lorro@Lorands-Mac-mini wg21_lorros_clang_uneval_proposal_implementation % 
lorro@Lorands-Mac-mini wg21_lorros_clang_uneval_proposal_implementation % cat clangtv/new.cpp 
#include <iostream>

bool f(bool b)
{
    std::cout << "f:" << b << std::endl;
    return b;
}

template<typename A, typename B>
bool either(A a, B b)
{
    if (a()) return true;
    return b();
}

int main()
{
    std::cout << either([&] { return f(true);}, [&] { return f(false);} ) << std::endl;
}
