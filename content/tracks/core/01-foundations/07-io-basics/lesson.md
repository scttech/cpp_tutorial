# I/O basics

## Output

You have used `std::cout`. Chain `<<` to send several things, and use `'\n'` for a newline:

```cpp
std::cout << "x = " << 42 << ", ratio = " << 0.5 << '\n';
```

Errors and diagnostics conventionally go to `std::cerr`, which is a separate stream from `std::cout`.

## Input with `std::cin`

`>>` reads one whitespace-separated token and converts it to the type of the variable:

```cpp
std::string name;
int age;
std::cin >> name >> age;       // input "Ada 36"  ->  name == "Ada", age == 36
```

This is convenient but **fragile**. Three things to know:

### 1. A failed extraction puts the stream in a fail state

```cpp
int n;
std::cin >> n;      // input "abc": extraction fails, n is set to 0, and the stream is now in a FAIL state
std::cin >> n;      // does NOTHING: all further reads fail until you call std::cin.clear() and discard the bad input
```

You can test a stream in a condition: `if (std::cin >> n) { /* it worked */ }`.

### 2. Reading a number then a line: the newline trap

```cpp
int age;
std::string full_name;
std::cin >> age;                 // reads "36" but leaves the '\n' after it in the buffer
std::getline(std::cin, full_name);   // reads up to the next '\n': which is IMMEDIATELY, so full_name is ""
```

Mixing `>>` and `getline` needs care. A common fix is to consume the leftover line first, with
`std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');`. Often the cleaner approach is to read
**whole lines** with `getline` and then parse each line yourself.

### 3. Reading until the input ends

```cpp
std::string line;
while (std::getline(std::cin, line)) {     // false at end of input (or on error)
    // process line
}
```

Put the read in the loop **condition**. A loop like `while (!std::cin.eof())` is a well-known bug: it runs one extra time.

## Parsing text into numbers

`std::stoi` throws an exception on bad input (and you have not met exceptions yet). The modern, non-throwing way is
`std::from_chars` from `<charconv>`:

```cpp
int value = 0;
std::string_view text = "123";
auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
bool ok = (ec == std::errc{}) && ptr == text.data() + text.size();   // parsed AND consumed everything
```

Checking that `ptr` reached the end rejects inputs such as `"30x"`, which `from_chars` would otherwise happily
parse as `30`.

## Your task

Write a program (in `main`) that reads **lines** from standard input. Each line is a person's name followed by their age,
for example `Ada Lovelace 36`. The name may contain spaces; the age is the **last** whitespace-separated token.

For each line:

- **Blank line** (empty or only spaces): ignore it. It counts as neither valid nor invalid.
- **Valid line**: the last token is a whole number from 0 to 150, and a non-empty name comes before it. Print
  `<name> is <age> years old`. The name is printed with surrounding whitespace removed.
- **Anything else**: print `Invalid line: <the line exactly as read>`.

After the last line print `Valid: <V>, Invalid: <I>`.

Example:

```
Input:                    Output:
Ada Lovelace 36           Ada Lovelace is 36 years old
Bob                       Invalid line: Bob
Carol x                   Invalid line: Carol x
                          Valid: 1, Invalid: 2
```

You will need `std::getline`, a way to find the last token, and `std::from_chars`. Use the **stdin** box under the
editor to try your program with the **Run** button.
