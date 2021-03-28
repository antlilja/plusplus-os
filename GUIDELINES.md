## Contributions guidlines
- Pull request for new features.
- Don't include random fixes in pull requests. The pull request should do one "thing".
- At least one person has to review the code before merging.
- You are reviewing the code and not the person.
- If a solution is temporary mention it in the pull request and create an issue.
- Issues should include links to the places (with exact line) in the code that you are talking about.

## Commenting guidelines
- Comment unclear code but don't comment on everything.
- Links in comments are encouraged.
- Tag TODOs with your name, the date and an issue number:
```c
// TODO(name, date, #issue) Thing needs to be fixed
void temporary_function() {
 // ...
}
```

## Code guidelines
- Use clang format and enable format on save.
- Exit early instead of nesting a bunch of if statements
- Don't split code up into too many functions, it can be grouped into scopes instead. But don't create HUGE functions.
- Every function should do one thing.
- c_ prefix for global constants.
- Structs:
```c
// Don't do this
struct FrameBuffer {
// ...
};
void foo(struct FrameBuffer* fb);

// Do this
typedef struct {
// ...
} FrameBuffer;
void foo(FrameBuffer* fb);
```
- Global variables (only define them in the header file if they are needed outside):
```c
// foo.h (In header file)
extern TYPE g_name;

// foo.c (In source file)
TYPE g_name;
```
