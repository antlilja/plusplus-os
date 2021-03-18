## Contributions guidlines
- Pull request for new features
- Small pull requests
- At least one person has to review the code before merging.
- Don't include random fixes in pull requests.
- You are reviewing the code and not the person.
- If a solution is temporary mention it in the pull request and create an issue.
- Issues should include links to the places (with exact line) you are talking about.

## Code guidelines
- Enable format on save (Use clang format)
- Comment unclear code but don't comment on everything. (Not too much noise)
- Links in comments are encouraged
- Exit early instead of nesting a bunch of if statements
- Group code in scopes
- Don't split code up into too many functions but don't create HUGE functions. 
- Every function should do one thing.
- Tag TODOs with your name, the date and an issue. (// TODO(name, date, issue number))
- g_ prefix for global variables.
- c_ prefix for global constants.
- Typedef structs:
```c
// Don't do this
struct FrameBuffer {
// FOOOOO
};
void foo(struct FrameBuffer* fb);

// Do this
typedef struct FrameBuffer {
// FOOOO
} FrameBuffer;
void foo(FrameBuffer* fb);
```
