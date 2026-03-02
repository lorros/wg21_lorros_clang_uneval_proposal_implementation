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
