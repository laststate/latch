import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("minify_sources.py")
SPEC = importlib.util.spec_from_file_location("minify_sources", MODULE_PATH)
MINIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MINIFY)


class MinifySourceTests(unittest.TestCase):
    def test_removes_comments_without_joining_tokens(self):
        source = "int main(void) { return/**/ 1 + 2; } // end\n"
        self.assertEqual(MINIFY.minify_source(source), "int main(void){return 1+2;}\n")

    def test_preserves_literals_and_directives(self):
        source = '#define VALUE(a, b) ((a) + (b))\\\n  /* keep behavior */\nconst char *s = "a // b";\n'
        expected = '#define VALUE(a, b) ((a) + (b))\\\n  /* keep behavior */\nconst char*s="a // b";\n'
        self.assertEqual(MINIFY.minify_source(source), expected)

    def test_does_not_create_new_operators(self):
        source = "int f(int a, int b) { return a + +b + (a > > b); }\n"
        self.assertEqual(MINIFY.minify_source(source), "int f(int a,int b){return a+ +b+(a> >b);}\n")

    def test_is_idempotent(self):
        source = "/* header */\nint value = 42;\n"
        minified = MINIFY.minify_source(source)
        self.assertEqual(MINIFY.minify_source(minified), minified)

    def test_minifies_rust_without_joining_keywords_and_literals(self):
        source = 'unsafe extern "C" { fn value(name: &\'static str) { let raw = br#"a\\\\b"#; } }\n'
        expected = 'unsafe extern "C"{fn value(name:&\'static str){let raw=br#"a\\\\b"#;}}\n'
        self.assertEqual(MINIFY.minify_rust_source(source), expected)

    def test_preserves_rust_character_literals_and_lifetimes(self):
        source = "let byte = '\\x7f'; let escaped = '\\n'; let name: &'static str = \"x\";\n"
        expected = "let byte='\\x7f';let escaped='\\n';let name:&'static str=\"x\";\n"
        self.assertEqual(MINIFY.minify_rust_source(source), expected)

    def test_minifies_assembly_without_merging_instructions(self):
        source = ".thumb\n\nlabel:\n  movs r0, #1\n  b label\n"
        expected = ".thumb\nlabel:\nmovs r0, #1\nb label\n"
        self.assertEqual(MINIFY.minify_assembly_source(source), expected)


if __name__ == "__main__":
    unittest.main()
