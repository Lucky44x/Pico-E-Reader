# MicroMD

MicroMD is a MarkDownDown language if you will... it uses a similar approach to Markdown itself, that being a minimal markup language, but focuses this on the E-Reader specific context, mainly trying to avoid accidental overlap with actual text and a highly reduced tag library

It uses the usual Escape character, that being `\`.

| Tag   | Usage                     | Explanation                   |
| -     | -                         | -                             |
| `<b>`  | `<b> BOLD </b>`          | Makes text bold               |
| `<i>`  | `<i> Italic </i>`        | Makes text italic             |
| `<u>`  | `<u> Underlined </u>`    | Underlines text               |
| `<d>`  | `<d> Dotted </d>`        | Dotted Underline              |
| `<s>`  | `<s> Strikethrough </s>` | Strikethrough text            |
| `<h>`  | `<h> HEADER </h>`        | Enlarges the Text-Size by x2  | 

As you can see, the tags are pretty much identical to HTML tags... this was done because HTML notation is the notation that is most human readable, while also being not very likely to collide with actual text in books

## Meta-Tags

There are also **Meta-Tags** Which are tags that result in an entire line of text being discarded and instead consumed as information for the Surrounding systems of the text-renderer. These tags do not require closing tags, as they are closed by the line they are on, but you can still write closing tags if you want to
Some Meta-Tags can also take optional data in their line of text, see below (some data may not be used depending on implementation of the parser/reader):

| Tag       | Usage | Explanation                                                                                   | Data          |
| -         | -     | -                                                                                             | -             |
| `<p>`     | `<p>` | Signals a Page-Break for the renderer. Text after this line will be forced onto a new page    | None          |
| `<c>`     | `<c>` | Signals the beginning of a Chapter. This is used by the quick-navigation of the reader        | Chapter Name  |

## Parser

The Parser uses the following approach:
1) Get as many lines of text as possible (best case of how many we can fit on screen) and buffer them in memory
2) Start parsing the lines character by character and feed state machine during this
3) Keep track of how much screen space is used and dynamically discard lines, until end of screen-space is reached
4) If Line-Contents are left over, cache them to memory and start the next page with them instead of directly reading the new lines

Here, each "Word" represents a token that is delimited by:
- Whitespace ` `
- Punctutation `. , ; :`
- Exclamation `! ?`

To minimize possible tag collisions it also follows these rules:
1) Only allow exact tags, that meaing `< x>` `<x` etc. are rendered literal, as they aren't valid
2) Allow escaping tags, so `\<b>` will not trigger a bold tag to be set
3) Do not consume standalone escape tags: `text <\b> text` will not consume the `<\b>` tag, as there was no opening tag

### Issues with this Approach
1) If Meta-Tags are used, we would either need to dynamically read lines when we encounter a meta-tag or we'd need to read more upfront just in case
2) Keeping track of screen space used would either require Direct access to Graphics libraries or a complex callback system
3) When a page break occurs before a tag is closed, that information has to be kept
4) When navigating, we cannot jump to specific pages because they don't follow a set size and we would have to precalculate the entire file for this
5) Generally, dynamic reading of lines would be better for buffer stabillity and performance, especially if we keep leftover text in the buffer and then just fill it back up when the time comes. This would increase efficiency but would also need a dynamic callback again
6) Chapter jumping would also require us to keep track of styles applied before page-break or chapter-break...

### Soltions
1) Just buffer 2 more lines than needed -> Ensures that both a Chapter tag and page break tag in succession are handeled... assume that this would be about the limit on average
2) Create a callback function `render_info render_word(word, style_sheet)` which is registered to the state machine by the user and takes the actual word and style_set, and returns a `render_info` struct which holds wether or not the word could be rendered, the line the word was rendered to, the remaining space on that line, whether or not a line break has occured during render and the space the word took up
3) Keep an internal style_sheet where styles are just set and reset based on the given tags. This allows for caching and keeping the last active tags from the previous page
4) Only allow jumps to the next page and further away `<c>` tags. Additionally, make all chapter tags also act as page-break tags, so that each chapter begins on it's own page
5) Implement additonal `get_line` callback, but keep all file-related logic in it's own manager
6) Keep a Chapter meta file which keeps byte offsets for each chapter and keep a Page meta file which keeps byte offsets, lengths and starting styles for each page. These meta files would have to be filled dynamically when the user navigates thus "Discovering" the files. Alternatively a discovery run at startup could be done if these mta files are not present by that point -> Requires more access to FileSystem again...

These Approaches would ensure that no cyclic dependencies and/or weird dependency trees are created, and keep seperation of responisbillities intact