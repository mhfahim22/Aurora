# Object-Oriented Programming

## Class Definition

```aura
class Person
    name = ""
    age = 0

    function greet()
        output("Hi, I'm " + self.name)

    function have_birthday()
        self.age += 1
```

`self` refers to the current instance (implied, not a keyword).

### Colon optional
```aura
class Person:
    name = ""
```

## Construction

```aura
p = Person("Alice", 30)          # positional args match field order
p2 = new Person("Bob", 25)       # explicit `new` keyword
p.greet()
```

## Inheritance

```aura
class Student extends Person
    school = "unknown"

    function study()
        output(self.name + " is studying")

s = Student("Bob", 20, "Aurora High")
s.greet()                          # inherited method
s.study()
```

## Abstract & Final

```aura
abstract class Shape
    function area()                # abstract method (no body)
    function describe()
        output("This is a shape")

final class Circle extends Shape
    radius = 0
    function area()
        return 3.14159 * self.radius * self.radius
```

## Interfaces

```aura
interface Drawable
    function draw()

interface Shape extends Drawable    # interface inheritance
    function area()

class Square implements Drawable
    function draw()
        output("Drawing square")

class Circle implements Drawable:
    function draw():
        output("Drawing circle")
```

## Visibility Modifiers

```aura
class Account
    public owner = ""              # accessible externally
    private balance = 0            # only inside class
    protected nickname = "none"    # class + subclasses

    public function deposit(amount)    # methods can also be public
        if amount > 0
            self.balance += amount
```

## Constructor Pattern

Aurora doesn't have explicit constructors. Construction uses positional arguments matching field declaration order, or named fields with `{ }` syntax.
