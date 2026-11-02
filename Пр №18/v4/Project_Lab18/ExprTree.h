/*
 * Практична робота №18
 * Тема: Розробка додатку для визначення похідної з використанням
 *       принципів об'єктно-орієнтованого програмування
 *
 * Модуль: ExprTree — ієрархія класів дерева математичного виразу.
 *
 * Автор: Максим Ковалишин
 * Copyright (c) 2026 Максим Ковалишин. Усі права захищено.
 *
 * Модуль НЕ залежить від VCL (лише стандартна бібліотека C++), тому його
 * можна перевіряти і використовувати окремо від графічного інтерфейсу.
 *
 * Ієрархія класів (рис. 1 з методички + додаткові завдання):
 *
 *                         Telement
 *     ┌──────┬─────┬──────┬───────┬──────┬─────┬─────┬─────┬─────┐
 *    Real   Var  Plus  Minus   Mult   Div   Neg   Sin   Cos
 *
 *  Real  — дійсне число            Var  — змінна (x або y)
 *  Plus, Minus, Mult, Div — арифметичні операції + - * /
 *  Neg   — унарний мінус           Sin, Cos — тригонометричні функції
 */
#ifndef ExprTreeH
#define ExprTreeH

#include <string>
#include <stdexcept>

namespace ExprLib {

//---------------------------------------------------------------------------
// Помилка розбору виразу (код помилки + позиція у рядку, починаючи з 1)
//---------------------------------------------------------------------------
class ParseError : public std::runtime_error
{
public:
    enum Code {
        EmptyExpression,   // порожній вираз
        UnexpectedChar,    // неочікуваний символ
        UnexpectedEnd,     // вираз обірвано
        MissingParen,      // немає закриваючої дужки
        ExpectedParen,     // після sin/cos очікується "("
        BadNumber,         // некоректне число
        UnknownName        // невідомий ідентифікатор
    };
    ParseError(Code c, int pos, const char* msg)
        : std::runtime_error(msg), FCode(c), FPos(pos) {}
    Code code() const { return FCode; }
    int  position() const { return FPos; }
private:
    Code FCode;
    int  FPos;
};

//---------------------------------------------------------------------------
// Базовий клас вершини дерева
//---------------------------------------------------------------------------
class Telement
{
protected:
    Telement *left, *right;
    Telement(Telement* L, Telement* R) : left(L), right(R) {}
private:
    Telement(const Telement&);             // копіювання заборонено —
    Telement& operator=(const Telement&);  // використовуйте copy()
public:
    virtual ~Telement() { delete left; delete right; }

    // значення піддерева
    virtual double rezult() const = 0;
    // глибока копія піддерева
    virtual Telement* copy() const = 0;
    // похідна піддерева за змінною v (повертає НОВЕ дерево)
    virtual Telement* differ(char v) const = 0;
    // як у методичці: differ() без аргументів - похідна за x
    // (виклик працює через вказівник на Telement)
    Telement* differ() const { return differ('x'); }
    // спрощене еквівалентне дерево (згортання констант, зведення подібних)
    virtual Telement* simplify() const = 0;
    // запис виразу у вигляді рядка; parentPrec — пріоритет батьківської вершини
    virtual std::string toString(int parentPrec = 0) const = 0;

    // надає змінній name значення X (виклик рекурсивно йде вниз по дереву)
    virtual void set_var(char name, double X)
    {
        if (left)  left->set_var(name, X);
        if (right) right->set_var(name, X);
    }
    // як у методичці: set_var(X) - значення змінної x
    void set_var(double X) { set_var('x', X); }
    // чи залежить піддерево від змінної v
    virtual bool dependsOn(char v) const
    {
        return (left && left->dependsOn(v)) || (right && right->dependsOn(v));
    }

    // допоміжні запити типу вершини (для спрощення виразів)
    virtual bool isReal(double& /*value*/) const { return false; }
    virtual bool isNeg() const { return false; }

    Telement* getLeft()  const { return left; }
    Telement* getRight() const { return right; }
};


//---------------------------------------------------------------------------
// Простий "розумний" вказівник на дерево (сумісний зі старим компілятором bcc32):
// сам знищує дерево в деструкторі, копіювання заборонено.
//---------------------------------------------------------------------------
class TreePtr
{
    Telement* p;
    TreePtr(const TreePtr&);
    TreePtr& operator=(const TreePtr&);
public:
    explicit TreePtr(Telement* t = 0) : p(t) {}
    ~TreePtr() { delete p; }
    Telement* get() const { return p; }
    Telement* operator->() const { return p; }
    Telement* release() { Telement* t = p; p = 0; return t; }
    void reset(Telement* t = 0) { if (t != p) { delete p; p = t; } }
};

// true, якщо число скінченне (не NaN і не ±INF) - незалежно від версії компілятора
inline bool isFinite(double v) { return v == v && (v - v) == 0.0; }

//---------------------------------------------------------------------------
class Real : public Telement
{
    double f;
public:
    explicit Real(double F) : Telement(0, 0), f(F) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
    bool isReal(double& value) const { value = f; return true; }
};

//---------------------------------------------------------------------------
class Var : public Telement
{
    char name;
    double x;
public:
    explicit Var(char Name = 'x', double X = 0) : Telement(0, 0), name(Name), x(X) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
    void set_var(char Name, double X);
    bool dependsOn(char v) const { return v == name; }
};

//---------------------------------------------------------------------------
class Plus : public Telement
{
public:
    Plus(Telement* L, Telement* R) : Telement(L, R) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
};

class Minus : public Telement
{
public:
    Minus(Telement* L, Telement* R) : Telement(L, R) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
};

class Mult : public Telement
{
public:
    Mult(Telement* L, Telement* R) : Telement(L, R) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
};

class Div : public Telement
{
public:
    Div(Telement* L, Telement* R) : Telement(L, R) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
};

//---------------------------------------------------------------------------
// Унарний мінус
class Neg : public Telement
{
public:
    explicit Neg(Telement* L) : Telement(L, 0) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
    bool isNeg() const { return true; }
};

// sin(u)
class Sin : public Telement
{
public:
    explicit Sin(Telement* L) : Telement(L, 0) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
};

// cos(u)
class Cos : public Telement
{
public:
    explicit Cos(Telement* L) : Telement(L, 0) {}
    double rezult() const;
    Telement* copy() const;
    Telement* differ(char v) const;
    Telement* simplify() const;
    std::string toString(int parentPrec = 0) const;
};

//---------------------------------------------------------------------------
// Вільні функції
//---------------------------------------------------------------------------

// Будує дерево об'єктів за рядком (функція form() з методички).
// Рядок аналізується справа наліво: спочатку шукається крайній правий
// "+" або "-" (поза дужками), потім "*" або "/"; знайдений оператор стає
// коренем дерева, а ліва й права частини будуються рекурсивно.
// Підтримує: + - * /, дужки, унарний мінус, дійсні числа ('.' або ','),
// змінні x та y, функції sin(...) і cos(...).
// Кидає ParseError. Власником результату є викликач (delete).
Telement* form(const std::string& s);

// n-та похідна за змінною v (n >= 0), результат уже спрощений.
// Власником результату є викликач (delete).
Telement* differN(const Telement* f, int n, char v = 'x');

// Число -> рядок (до 12 значущих цифр, крапка як роздільник, без експоненти)
std::string fmtNumber(double value);

} // namespace ExprLib

#endif
