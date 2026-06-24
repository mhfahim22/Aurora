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

### Brace blocks also work
```aura
class Person {
    name = ""
    age = 0

    function greet() {
        output("Hi, I'm " + self.name)
    }
}
```

## Construction

```aura
p = Person("Alice", 30)          # positional args match field order
p2 = new Person("Bob", 25)       # explicit `new` keyword
p3 = Person { name: "Charlie" }  # named construction (unspecified fields get defaults)
p.greet()
```

## `self` and `super`

`self` refers to the current instance. `super` refers to the parent class:

```aura
class Animal
    function speak()
        output("generic")

class Dog extends Animal
    function speak()
        super.speak()           # call parent method
        output("woof")
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

### Abstract / Final classes

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

### Abstract / Final methods

```aura
class Document
    abstract function render()     # subclasses must implement
    final function getId()         # subclasses cannot override
        return self.id
```

## Static Members

```aura
class Counter
    static count = 0               # shared across all instances

    static function reset()
        self.count = 0
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

### Multiple Interfaces

```aura
interface Serializable
    function serialize()

class Report implements Drawable, Serializable
    function draw()
        output("drawing report")
    function serialize()
        output("serializing report")
```

## Visibility Modifiers

| Modifier    | Access Scope          |
|-------------|----------------------|
| `public`    | Anywhere (default)    |
| `private`   | Only within class     |
| `protected` | Class + subclasses    |

```aura
class Account
    public owner = ""              # accessible externally
    private balance = 0            # only inside class
    protected nickname = "none"    # class + subclasses

    public function deposit(amount)    # methods can also be public
        if amount > 0
            self.balance += amount

    private function log_tx(type)
        output("tx: " + type)

    protected function get_nickname()
        return self.nickname
```

## Constructor Pattern

Aurora doesn't have explicit constructors. Construction uses positional arguments matching field declaration order, or named fields with `{ }` syntax. Field default values are used for any missing arguments.

```aura
p = Person("Alice", 30)          # positional
p2 = new Person("Bob")           # `new` keyword, one field gets default
p3 = Person { name: "Charlie" }  # named fields
```

---

**Next:** [Structs, Enums, Unions](08-structs-enums-unions.md)
