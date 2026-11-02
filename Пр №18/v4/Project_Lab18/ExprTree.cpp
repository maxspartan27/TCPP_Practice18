/*
 * Практична робота №18 — модуль ExprTree (реалізація)
 *
 * Автор: Максим Ковалишин
 * Copyright (c) 2026 Максим Ковалишин. Усі права захищено.
 */
#pragma hdrstop
#include "ExprTree.h"

#include <cmath>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <utility>
#include <algorithm>

namespace ExprLib {

//===========================================================================
//  Форматування чисел
//===========================================================================
std::string fmtNumber(double value)
{
    if (value == 0) return "0";
    char buf[64];
    std::sprintf(buf, "%.12g", value);
    if (std::strchr(buf, 'e') || std::strchr(buf, 'E'))   // без експоненти: form() її не розбирає
        std::sprintf(buf, "%.15f", value);
    std::string r(buf);
    if (r.find('.') != std::string::npos || r.find(',') != std::string::npos)
    {
        size_t e = r.find_last_not_of('0');                // прибираємо кінцеві нулі
        if (e != std::string::npos) r.erase(e + 1);
        if (!r.empty() && (r[r.size() - 1] == '.' || r[r.size() - 1] == ',')) r.erase(r.size() - 1);
    }
    for (size_t i = 0; i < r.size(); ++i)      // десятковий роздільник - завжди крапка
        if (r[i] == ',') r[i] = '.';
    return r;
}

// Пріоритети: 1 — + -;  2 — * /;  3 — унарний мінус;  5 — атоми
static std::string wrap(bool need, const std::string& s)
{
    return need ? "(" + s + ")" : s;
}

//===========================================================================
//  Допоміжні функції для спрощення виразів
//===========================================================================
// Розкладає добуток на числовий коефіцієнт c і список нечислових множників.
static void flatten(const Telement* t, double& c, std::vector<const Telement*>& f)
{
    double v;
    if (t->isReal(v)) { c *= v; return; }
    if (t->isNeg())   { c = -c; flatten(t->getLeft(), c, f); return; }
    if (dynamic_cast<const Mult*>(t))
    {
        flatten(t->getLeft(),  c, f);
        flatten(t->getRight(), c, f);
        return;
    }
    f.push_back(t);
}

// Ключ добутку (множники впорядковані, бо множення комутативне)
static std::string keyOf(const std::vector<const Telement*>& f)
{
    std::vector<std::string> k;
    for (size_t i = 0; i < f.size(); ++i) k.push_back(f[i]->toString());
    std::sort(k.begin(), k.end());
    std::string r;
    for (size_t i = 0; i < k.size(); ++i) { if (i) r += "*"; r += k[i]; }
    return r;
}

// Будує дерево c * f1 * f2 * ...
static Telement* buildProduct(double c, const std::vector<const Telement*>& f)
{
    if (c == 0) return new Real(0);
    if (f.empty()) return new Real(c);
    double a = std::fabs(c);
    Telement* acc = 0;
    size_t i = 0;
    if (a == 1) { acc = f[0]->copy(); i = 1; }
    else        { acc = new Real(a); }
    for (; i < f.size(); ++i) acc = new Mult(acc, f[i]->copy());
    if (c < 0) acc = new Neg(acc);
    return acc;
}

// Збирає доданки суми (з урахуванням знаку) у плоский список.
static void collect(const Telement* t, int sign,
                    std::vector<std::pair<int, const Telement*> >& out)
{
    if (dynamic_cast<const Plus*>(t))
    {
        collect(t->getLeft(),  sign, out);
        collect(t->getRight(), sign, out);
    }
    else if (dynamic_cast<const Minus*>(t))
    {
        collect(t->getLeft(),  sign, out);
        collect(t->getRight(), -sign, out);
    }
    else if (t->isNeg())
        collect(t->getLeft(), -sign, out);
    else
        out.push_back(std::make_pair(sign, t));
}

struct STerm
{
    double c;
    std::vector<const Telement*> f;
    std::string key;
};

// Спрощує суму l + rsign*r: зводить подібні доданки, складає константи.
// Бере у власність і знищує l та r.
static Telement* simplifySum(Telement* l, Telement* r, int rsign)
{
    std::vector<std::pair<int, const Telement*> > items;
    collect(l, +1, items);
    collect(r, rsign, items);

    std::vector<STerm> terms;
    for (size_t i = 0; i < items.size(); ++i)
    {
        STerm t;
        t.c = items[i].first;
        flatten(items[i].second, t.c, t.f);
        t.key = keyOf(t.f);
        bool merged = false;
        for (size_t j = 0; j < terms.size(); ++j)
            if (terms[j].key == t.key) { terms[j].c += t.c; merged = true; break; }
        if (!merged) terms.push_back(t);
    }

    Telement* res = 0;
    for (size_t i = 0; i < terms.size(); ++i)
    {
        if (terms[i].c == 0) continue;
        if (!res)
            res = buildProduct(terms[i].c, terms[i].f);
        else
        {
            Telement* body = buildProduct(std::fabs(terms[i].c), terms[i].f);
            if (terms[i].c < 0) res = new Minus(res, body);
            else                res = new Plus(res, body);
        }
    }
    if (!res) res = new Real(0);
    delete l;
    delete r;
    return res;
}

//===========================================================================
//  Real
//===========================================================================
double Real::rezult() const { return f; }
Telement* Real::copy() const { return new Real(f); }
Telement* Real::differ(char) const { return new Real(0); }   // (c)' = 0
Telement* Real::simplify() const { return new Real(f); }
std::string Real::toString(int parentPrec) const
{
    return wrap(f < 0 && parentPrec >= 2, fmtNumber(f));
}

//===========================================================================
//  Var
//===========================================================================
double Var::rezult() const { return x; }
Telement* Var::copy() const { return new Var(name, x); }
Telement* Var::differ(char v) const { return new Real(v == name ? 1 : 0); }
Telement* Var::simplify() const { return new Var(name, x); }
std::string Var::toString(int) const { return std::string(1, name); }
void Var::set_var(char Name, double X) { if (Name == name) x = X; }

//===========================================================================
//  Plus:  (u + v)' = u' + v'
//===========================================================================
double Plus::rezult() const { return left->rezult() + right->rezult(); }
Telement* Plus::copy() const { return new Plus(left->copy(), right->copy()); }
Telement* Plus::differ(char v) const
{
    return new Plus(left->differ(v), right->differ(v));
}
Telement* Plus::simplify() const
{
    return simplifySum(left->simplify(), right->simplify(), +1);
}
std::string Plus::toString(int parentPrec) const
{
    return wrap(parentPrec > 1, left->toString(1) + " + " + right->toString(1));
}

//===========================================================================
//  Minus:  (u - v)' = u' - v'
//===========================================================================
double Minus::rezult() const { return left->rezult() - right->rezult(); }
Telement* Minus::copy() const { return new Minus(left->copy(), right->copy()); }
Telement* Minus::differ(char v) const
{
    return new Minus(left->differ(v), right->differ(v));
}
Telement* Minus::simplify() const
{
    return simplifySum(left->simplify(), right->simplify(), -1);
}
std::string Minus::toString(int parentPrec) const
{
    return wrap(parentPrec > 1, left->toString(1) + " - " + right->toString(2));
}

//===========================================================================
//  Mult:  (u * v)' = u' * v + u * v'
//===========================================================================
double Mult::rezult() const { return left->rezult() * right->rezult(); }
Telement* Mult::copy() const { return new Mult(left->copy(), right->copy()); }
Telement* Mult::differ(char v) const
{
    return new Plus(new Mult(left->differ(v), right->copy()),
                    new Mult(left->copy(), right->differ(v)));
}
Telement* Mult::simplify() const
{
    Telement* l = left->simplify();
    Telement* r = right->simplify();
    double c = 1;
    std::vector<const Telement*> f;
    flatten(l, c, f);
    flatten(r, c, f);
    Telement* res = (c == 0) ? new Real(0) : buildProduct(c, f);
    delete l;
    delete r;
    return res;
}
std::string Mult::toString(int parentPrec) const
{
    return wrap(parentPrec > 2, left->toString(2) + "*" + right->toString(2));
}

//===========================================================================
//  Div:  (u / v)' = (u' * v - u * v') / (v * v)
//===========================================================================
double Div::rezult() const { return left->rezult() / right->rezult(); }
Telement* Div::copy() const { return new Div(left->copy(), right->copy()); }
Telement* Div::differ(char v) const
{
    return new Div(new Minus(new Mult(left->differ(v), right->copy()),
                             new Mult(left->copy(), right->differ(v))),
                   new Mult(right->copy(), right->copy()));
}
Telement* Div::simplify() const
{
    Telement* l = left->simplify();
    Telement* r = right->simplify();
    double a, b;
    if (l->isReal(a) && r->isReal(b) && b != 0)
    { delete l; delete r; return new Real(a / b); }
    if (r->isReal(b) && b == 1)
    { delete r; return l; }

    // розкладаємо чисельник і знаменник на коефіцієнт та множники
    double cl = 1, cr = 1;
    std::vector<const Telement*> fl, fr;
    flatten(l, cl, fl);
    flatten(r, cr, fr);
    if (cr == 0) return new Div(l, r);                 // ділення на нуль залишаємо як є
    if (cl == 0) { delete l; delete r; return new Real(0); }

    // скорочуємо однакові множники чисельника та знаменника
    for (size_t i = 0; i < fl.size(); )
    {
        std::string k = fl[i]->toString();
        size_t j = 0;
        for (; j < fr.size(); ++j) if (fr[j]->toString() == k) break;
        if (j < fr.size()) { fl.erase(fl.begin() + i); fr.erase(fr.begin() + j); }
        else ++i;
    }

    int sign = 1;
    if (cl < 0) { sign = -sign; cl = -cl; }
    if (cr < 0) { sign = -sign; cr = -cr; }

    Telement* num = buildProduct(cl, fl);
    Telement* res;
    if (fr.empty() && cr == 1) res = num;
    else res = new Div(num, buildProduct(cr, fr));
    if (sign < 0) res = new Neg(res);
    delete l;
    delete r;
    return res;
}
std::string Div::toString(int parentPrec) const
{
    return wrap(parentPrec > 2, left->toString(2) + "/" + right->toString(3));
}

//===========================================================================
//  Neg:  (-u)' = -u'
//===========================================================================
double Neg::rezult() const { return -left->rezult(); }
Telement* Neg::copy() const { return new Neg(left->copy()); }
Telement* Neg::differ(char v) const { return new Neg(left->differ(v)); }
Telement* Neg::simplify() const
{
    Telement* a = left->simplify();
    if (dynamic_cast<Plus*>(a) || dynamic_cast<Minus*>(a))
        return simplifySum(new Real(0), a, -1);
    double c = -1;
    std::vector<const Telement*> f;
    flatten(a, c, f);
    Telement* res = buildProduct(c, f);
    delete a;
    return res;
}
std::string Neg::toString(int parentPrec) const
{
    return wrap(parentPrec >= 2, "-" + left->toString(2));
}

//===========================================================================
//  Sin:  (sin u)' = cos(u) * u'
//===========================================================================
double Sin::rezult() const { return std::sin(left->rezult()); }
Telement* Sin::copy() const { return new Sin(left->copy()); }
Telement* Sin::differ(char v) const
{
    return new Mult(new Cos(left->copy()), left->differ(v));
}
Telement* Sin::simplify() const
{
    Telement* a = left->simplify();
    double v;
    if (a->isReal(v)) { delete a; return new Real(std::sin(v)); }
    return new Sin(a);
}
std::string Sin::toString(int) const { return "sin(" + left->toString(0) + ")"; }

//===========================================================================
//  Cos:  (cos u)' = -sin(u) * u'
//===========================================================================
double Cos::rezult() const { return std::cos(left->rezult()); }
Telement* Cos::copy() const { return new Cos(left->copy()); }
Telement* Cos::differ(char v) const
{
    return new Neg(new Mult(new Sin(left->copy()), left->differ(v)));
}
Telement* Cos::simplify() const
{
    Telement* a = left->simplify();
    double v;
    if (a->isReal(v)) { delete a; return new Real(std::cos(v)); }
    return new Cos(a);
}
std::string Cos::toString(int) const { return "cos(" + left->toString(0) + ")"; }

//===========================================================================
//  n-та похідна
//===========================================================================
Telement* differN(const Telement* f, int n, char v)
{
    Telement* cur = f->simplify();
    for (int i = 0; i < n; ++i)
    {
        Telement* d = cur->differ(v);
        delete cur;
        cur = d->simplify();
        delete d;
    }
    return cur;
}

//===========================================================================
//  Розбір виразу - функція form() з методички.
//
//  Рядок аналізується СПРАВА НАЛІВО (операції одного пріоритету виконуються
//  зліва направо, тому корінь дерева - це останній виконуваний оператор, тобто
//  крайній правий у виразі):
//     1) крайній правий "+" або "-"  поза дужками  -> Plus / Minus
//     2) крайній правий "*" або "/"  поза дужками  -> Mult / Div
//     3) унарний "-" (або "+") на початку            -> Neg
//     4) вираз у дужках (...)                        -> розбір вмісту
//     5) sin(...) / cos(...)                         -> Sin / Cos
//     6) x або y                                     -> Var
//     7) інакше - дійсне число                       -> Real
//  Ліву й праву частини будуємо рекурсивно (form(s1), form(s2)).
//===========================================================================
namespace {

class Former
{
    std::string s;            // вираз без пробілів
    std::vector<int> orig;    // orig[i] - позиція i-го символу в початковому рядку (з 1)

    static bool isDigit(char c) { return c >= '0' && c <= '9'; }
    static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
    // чи може символ закінчувати операнд (тоді наступний +/- бінарний)
    static bool endsOperand(char c)
    { return isDigit(c) || isAlpha(c) || c == '.' || c == ',' || c == ')'; }
    // чи може символ входити до операнда (число, ім'я)
    static bool inOperand(char c)
    { return isDigit(c) || isAlpha(c) || c == '.' || c == ','; }

    int at(size_t i) const { return i < orig.size() ? orig[i] : (orig.empty() ? 1 : orig.back() + 1); }

    // позиція з кінця (аналог PosFromEnd з методички) першого з символів
    // a або b у піддіапазоні [lo,hi), що лежить поза дужками й є бінарним
    // оператором; -1, якщо такого немає
    int posFromEnd(int lo, int hi, char a, char b) const
    {
        int depth = 0;
        for (int i = hi - 1; i > lo; --i)
        {
            char c = s[i];
            if (c == ')') ++depth;
            else if (c == '(') --depth;
            else if (depth == 0 && (c == a || c == b))
            {
                if (a == '+' && !endsOperand(s[i - 1])) continue;   // унарний знак
                return i;
            }
        }
        return -1;
    }

    // чи дужка s[lo] закривається саме на s[hi-1]
    bool wrapsAll(int lo, int hi) const
    {
        int depth = 0;
        for (int i = lo; i < hi; ++i)
        {
            if (s[i] == '(') ++depth;
            else if (s[i] == ')')
            {
                --depth;
                if (depth == 0) return i == hi - 1;
            }
        }
        return false;
    }

    Telement* number(int lo, int hi) const
    {
        double ip = 0, fp = 0, scale = 1;
        int digits = 0, seps = 0;
        for (int i = lo; i < hi; ++i)
        {
            char c = s[i];
            if (c == '.' || c == ',')
            {
                if (++seps > 1) throw ParseError(ParseError::BadNumber, at(lo), "bad number");
            }
            else if (isDigit(c))
            {
                ++digits;
                if (seps == 0) ip = ip * 10 + (c - '0');
                else { fp = fp * 10 + (c - '0'); scale *= 10; }
            }
            else throw ParseError(ParseError::UnexpectedChar, at(i), "unexpected character");
        }
        if (digits == 0) throw ParseError(ParseError::BadNumber, at(lo), "bad number");
        return new Real(ip + fp / scale);
    }

    Telement* build(int lo, int hi) const
    {
        if (lo >= hi)
            throw ParseError(ParseError::UnexpectedEnd, at(lo), "unexpected end");
        int p;
        TreePtr a, b;

        if ((p = posFromEnd(lo, hi, '+', '-')) > lo)
        {
            a.reset(build(lo, p));
            b.reset(build(p + 1, hi));
            Telement* l = a.release(); Telement* r = b.release();
            if (s[p] == '+') return new Plus(l, r);
            return new Minus(l, r);
        }
        if ((p = posFromEnd(lo, hi, '*', '/')) > lo)
        {
            a.reset(build(lo, p));
            b.reset(build(p + 1, hi));
            Telement* l = a.release(); Telement* r = b.release();
            if (s[p] == '*') return new Mult(l, r);
            return new Div(l, r);
        }
        if (s[lo] == '*' || s[lo] == '/')
            throw ParseError(ParseError::UnexpectedChar, at(lo), "unexpected character");
        if (s[lo] == '-')
        {
            a.reset(build(lo + 1, hi));
            return new Neg(a.release());
        }
        if (s[lo] == '+') return build(lo + 1, hi);

        if (s[lo] == '(')
        {
            if (!wrapsAll(lo, hi))
                throw ParseError(ParseError::UnexpectedChar, at(lo), "unexpected character");
            return build(lo + 1, hi - 1);
        }

        if (isAlpha(s[lo]))
        {
            int e = lo;
            std::string id;
            while (e < hi && isAlpha(s[e]))
            {
                char ch = s[e++];
                if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
                id += ch;
            }
            if (e == hi)
            {
                if (id == "x" || id == "y") return new Var(id[0]);
                if (id == "sin" || id == "cos")
                    throw ParseError(ParseError::ExpectedParen, at(hi), "expected (");
                throw ParseError(ParseError::UnknownName, at(lo), "unknown name");
            }
            if (id != "sin" && id != "cos")
            {
                if (s[e] == '(' || inOperand(s[e]))
                    throw ParseError(ParseError::UnknownName, at(lo), "unknown name");
                throw ParseError(ParseError::UnexpectedChar, at(e), "unexpected character");
            }
            if (s[e] != '(')
                throw ParseError(ParseError::ExpectedParen, at(e), "expected (");
            if (!wrapsAll(e, hi))
                throw ParseError(ParseError::UnexpectedChar, at(e), "unexpected character");
            a.reset(build(e + 1, hi - 1));
            if (id == "sin") return new Sin(a.release());
            return new Cos(a.release());
        }
        if (isDigit(s[lo]) || s[lo] == '.' || s[lo] == ',')
            return number(lo, hi);
        throw ParseError(ParseError::UnexpectedChar, at(lo), "unexpected character");
    }

public:
    explicit Former(const std::string& str)
    {
        // прибираємо пробіли, запам'ятовуючи позиції; пробіл між двома
        // частинами одного операнда ("2 3", "sin x") - помилка
        for (size_t i = 0; i < str.size(); ++i)
        {
            char c = str[i];
            if (c == ' ' || c == '\t')
            {
                size_t j = i;
                while (j < str.size() && (str[j] == ' ' || str[j] == '\t')) ++j;
                if (!s.empty() && j < str.size() && inOperand(s[s.size() - 1]) && inOperand(str[j]))
                    throw ParseError(ParseError::UnexpectedChar, (int)j + 1, "unexpected character");
                i = j - 1;
                continue;
            }
            s += c;
            orig.push_back((int)i + 1);
        }
    }

    Telement* parseAll() const
    {
        if (s.empty())
            throw ParseError(ParseError::EmptyExpression, 1, "empty expression");
        int depth = 0;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '(') ++depth;
            else if (s[i] == ')' && --depth < 0)
                throw ParseError(ParseError::UnexpectedChar, at(i), "unexpected character");
        }
        if (depth > 0)
            throw ParseError(ParseError::MissingParen, at(s.size()), "missing )");
        return build(0, (int)s.size());
    }
};

} // anonymous namespace

Telement* form(const std::string& str)
{
    Former f(str);
    return f.parseAll();
}

} // namespace ExprLib
