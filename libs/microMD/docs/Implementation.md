# Implementation

## Algorithm
1) Fill a ring buffer with bytes from the current file
2) Read characters from bytes and consume them into the state machine
3) once state machine finishes, call rendering function and wait for result
4) When successfull, fill up buffer and restart state machine
5) When unsuccessfull (page is full), cache word's codepoint and stylesheet and wait for next call
6) When new page selected, repeat

## Statemachine
For the state machine we need to define a few variables:
- Text-Buffer: a buffer containing the entire data of a page
- Word-Buffer: a buffer containing the characters of the word that is currently being assembled
- Tag-Buffer: a buffer that temporarily stores characters when handling tags
- StyleSheet: a collection of states of the different text-styling effects that are supported
- Escape-Flag: a simple flag, which determines if a character is interpreted or pushed to the word
- Has-Space-Left: a flag, set by the render call, which determines if we have used all of our space on the page, if so, we stop the state machine

We also need to define a few Characters
- End-Chars: All punctuation (. , ; :), exclamation (! ?) chars, and space
- Tag-Chars: < > for opening and closing a tag and [b,u,d,i etc.] for each style
- Escape Char: The escape character \

```mermaid
stateDiagram-v2
    direction TB

    state char_choice <<choice>>
    state space_left_choice <<choice>>
    state buffered_word_choice <<choice>>

    LOOP_TAG : [[RESTART STATE MACHINE]]
    LOOP_ESCAPE : [[RESTART STATE MACHINE]]
    LOOP_START : [[RESTART STATE MACHINE]]
    LOOP_NORMAL : [[RESTART STATE MACHINE]]
    LOOP_RENDER : [[RESTART STATE MACHINE]]

    RENDER_BUFFERED_WORD : [[ PERFORM_RENDER ]]

    RENDER_METHOD : [[ PERFORM_RENDER ]]

    start_read : Read Char

    [*] --> space_left_choice
    space_left_choice --> buffered_word_choice : Has Space Left on Render-Page
    space_left_choice --> [*] : No More Space on Render-Page

    buffered_word_choice --> RENDER_BUFFERED_WORD : Has a Buffered Word remaining
    RENDER_BUFFERED_WORD --> LOOP_START
    buffered_word_choice --> start_read : Has no Buffered Word remaining
    start_read --> char_choice

    %% Rendering
    RENDER_METHOD --> Render_State
    state Render_State {
        state render_fork_result <<fork>>
        state render_join_result <<join>>

        [*] --> Send_Word_To_Render_With_Style
        Send_Word_To_Render_With_Style --> render_fork_result
        render_fork_result --> Clear_Word_Buffer : Success
        Clear_Word_Buffer --> render_join_result

        render_fork_result --> Set_No_Space_Left : Failure
        Set_No_Space_Left --> render_join_result

        render_join_result --> [*]
    }
    Render_State --> LOOP_RENDER

    %% General Characters
    char_choice --> Normal_Character : any other Character
    state Normal_Character {
        state normal_choice_end_char <<choice>>
        normal_push : Push to Word
        NORMAL_RENDER : [[ PERFORM_RENDER ]]

        [*] --> normal_push
        normal_push --> Unset_Escape_Flag
        Unset_Escape_Flag --> normal_choice_end_char
        normal_choice_end_char --> [*] : any other character
        normal_choice_end_char --> NORMAL_RENDER : ending-character
        NORMAL_RENDER --> [*]
    }
    Normal_Character --> LOOP_NORMAL

    %% Tag Handling
    char_choice --> Tag_Character : <
    state Tag_Character {
        state fork_tag_entry <<fork>>
        state join_tag_entry <<join>>
        state fork_tag_esc <<fork>>
        state join_tag_esc <<join>>

        tag_push_esc : Push To Word
        entry_push_tag_buffer : Push To Tag
        tag_read_char : Read Char
        tag_read_char_unset : Read Char and Push to Tag
        tag_read_char_end : Read Char and Push to Tag
        tag_unset_esc : Unset Escape Flag

        [*] --> Clear_Tag_buffer 
        Clear_Tag_buffer --> fork_tag_entry
        %% Tag - Escape Flag unset
        fork_tag_entry --> entry_push_tag_buffer : Escape Flag unset
        entry_push_tag_buffer --> tag_read_char
        %% Check for inversion or tag set
        tag_read_char --> fork_tag_esc
        fork_tag_esc --> tag_read_char_unset : Unset Character ' / '
        fork_tag_esc --> Queue_Set_Style : any Style-Char
        tag_read_char_unset --> Queue_Unset_Style : any Style-Char
        Queue_Unset_Style --> join_tag_esc
        Queue_Set_Style --> join_tag_esc
        join_tag_esc --> tag_read_char_end
        tag_read_char_end --> join_tag_entry : >

        %% Tag - Escape Flag set
        fork_tag_entry --> tag_push_esc : Escape Flag Set
        tag_push_esc --> tag_unset_esc
        tag_unset_esc --> join_tag_entry

        join_tag_entry --> [*]
    }
    note right of Tag_Character
        When any transition in this StateMachine fails -> Wrong character used,
        this will end the state-machine and return out with an invalid tag-state
        which will push the temporary tag-buffer to the word-buffer and start the state-machine again
    end note
    state fork_tag_end <<fork>>
    state join_tag_end <<join>>
    tag_send_to_render : [[ PERFORM_RENDER ]]

    Tag_Character --> fork_tag_end
    fork_tag_end --> tag_send_to_render : Valid Tag
    tag_send_to_render --> Apply_Style_Sheet
    Apply_Style_Sheet --> join_tag_end
    fork_tag_end --> Push_Tag_To_Word : Invalid Tag
    Push_Tag_To_Word --> join_tag_end
    join_tag_end --> LOOP_TAG

    %% Escape Character handling
    char_choice --> Escape_Character : Escape Character ' \ '
    state Escape_Character {
        state fork_escape_entry <<fork>>
        state join_escape_entry <<join>>

        esc_push : Push To Word
        esc_unset_esc : Unset Escape Flag

        [*] --> fork_escape_entry
        fork_escape_entry --> Set_Escape_Flag : Escape Flag Unset
        fork_escape_entry --> esc_push : Escape Flag Set
        esc_push --> esc_unset_esc
        esc_unset_esc --> join_escape_entry
        Set_Escape_Flag --> join_escape_entry
        join_escape_entry --> [*]
    }
    Escape_Character --> LOOP_ESCAPE

```