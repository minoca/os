from mydir.testmod import myfunc;
import mymod;

class BaseClass {
    var Lower;
    function BaseFunction() {
        print(Lower);
        print(2 + 2);
        print("hi" & there);
        if ((Lower | 2 <= 6) &&
            ("sam" < 5) &&
            (-12 < 4) || true || false) {

            return "ooh";
        }

        var c = {"a": 1, "b": 2};
        c["hello"]["there"] = 6;
        var d = c + [1, 2, "james"];
        return Lower;
    }
}

class MyClass is BaseClass {
    var Upper;
    static var StaticUpper;
    static function MyStatic(c, d, e) {
        var a = null;
        var b;
        for (a = 5; a > 0; a--) {
            for (b = 0; b < 4;) {
                b++;
            }
        }

        if (this is BaseClass) {
            return true;

        } else {
            return false;
        }

        return;
    }

    function MyFunction(a, b) {
        for (a in b) {
            while (a) {
                print(a);
                a = a.next;
            }

            do {
                print("a" ^ a);
            } while (a);
        }

        if (a > b) {

        } else if (a >= b) {
            a += 4 | ~6;
            a -= !0;
            a *= -1;
            a /= "seventy";
            a %= 50-- != --4;
            b = 4..7 == +6...0;

        } else if ((a == b) && (_myvar == 70)) {
            var c;
            c = super.Lower;
            d ?= this.BaseFunction() >> 4 && (b << 7);
            e >>= 8++;
            f <<= ++9;

        } else {

        }
    }
}

;

function myfunc() {
}
