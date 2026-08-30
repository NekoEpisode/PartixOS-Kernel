# x86_64 ISR stubs — 256 entries + common handler
# Vectors 0-7, 9, 15-16, 18-20, 22-28, 31-255: no error code, push dummy 0
# Vectors 8, 10-14, 17, 21, 29-30: CPU pushes error code, don't push dummy

.code64
.section .text

.macro ISR_NOERR n
.align 8
.globl isr_\n
isr_\n:
    pushq $0
    pushq $\n
    jmp isr_common
.endm

.macro ISR_ERR n
.align 8
.globl isr_\n
isr_\n:
    pushq $\n
    jmp isr_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47
ISR_NOERR 48
ISR_NOERR 49
ISR_NOERR 50
ISR_NOERR 51
ISR_NOERR 52
ISR_NOERR 53
ISR_NOERR 54
ISR_NOERR 55
ISR_NOERR 56
ISR_NOERR 57
ISR_NOERR 58
ISR_NOERR 59
ISR_NOERR 60
ISR_NOERR 61
ISR_NOERR 62
ISR_NOERR 63
ISR_NOERR 64
ISR_NOERR 65
ISR_NOERR 66
ISR_NOERR 67
ISR_NOERR 68
ISR_NOERR 69
ISR_NOERR 70
ISR_NOERR 71
ISR_NOERR 72
ISR_NOERR 73
ISR_NOERR 74
ISR_NOERR 75
ISR_NOERR 76
ISR_NOERR 77
ISR_NOERR 78
ISR_NOERR 79
ISR_NOERR 80
ISR_NOERR 81
ISR_NOERR 82
ISR_NOERR 83
ISR_NOERR 84
ISR_NOERR 85
ISR_NOERR 86
ISR_NOERR 87
ISR_NOERR 88
ISR_NOERR 89
ISR_NOERR 90
ISR_NOERR 91
ISR_NOERR 92
ISR_NOERR 93
ISR_NOERR 94
ISR_NOERR 95
ISR_NOERR 96
ISR_NOERR 97
ISR_NOERR 98
ISR_NOERR 99
ISR_NOERR 100
ISR_NOERR 101
ISR_NOERR 102
ISR_NOERR 103
ISR_NOERR 104
ISR_NOERR 105
ISR_NOERR 106
ISR_NOERR 107
ISR_NOERR 108
ISR_NOERR 109
ISR_NOERR 110
ISR_NOERR 111
ISR_NOERR 112
ISR_NOERR 113
ISR_NOERR 114
ISR_NOERR 115
ISR_NOERR 116
ISR_NOERR 117
ISR_NOERR 118
ISR_NOERR 119
ISR_NOERR 120
ISR_NOERR 121
ISR_NOERR 122
ISR_NOERR 123
ISR_NOERR 124
ISR_NOERR 125
ISR_NOERR 126
ISR_NOERR 127
ISR_NOERR 128
ISR_NOERR 129
ISR_NOERR 130
ISR_NOERR 131
ISR_NOERR 132
ISR_NOERR 133
ISR_NOERR 134
ISR_NOERR 135
ISR_NOERR 136
ISR_NOERR 137
ISR_NOERR 138
ISR_NOERR 139
ISR_NOERR 140
ISR_NOERR 141
ISR_NOERR 142
ISR_NOERR 143
ISR_NOERR 144
ISR_NOERR 145
ISR_NOERR 146
ISR_NOERR 147
ISR_NOERR 148
ISR_NOERR 149
ISR_NOERR 150
ISR_NOERR 151
ISR_NOERR 152
ISR_NOERR 153
ISR_NOERR 154
ISR_NOERR 155
ISR_NOERR 156
ISR_NOERR 157
ISR_NOERR 158
ISR_NOERR 159
ISR_NOERR 160
ISR_NOERR 161
ISR_NOERR 162
ISR_NOERR 163
ISR_NOERR 164
ISR_NOERR 165
ISR_NOERR 166
ISR_NOERR 167
ISR_NOERR 168
ISR_NOERR 169
ISR_NOERR 170
ISR_NOERR 171
ISR_NOERR 172
ISR_NOERR 173
ISR_NOERR 174
ISR_NOERR 175
ISR_NOERR 176
ISR_NOERR 177
ISR_NOERR 178
ISR_NOERR 179
ISR_NOERR 180
ISR_NOERR 181
ISR_NOERR 182
ISR_NOERR 183
ISR_NOERR 184
ISR_NOERR 185
ISR_NOERR 186
ISR_NOERR 187
ISR_NOERR 188
ISR_NOERR 189
ISR_NOERR 190
ISR_NOERR 191
ISR_NOERR 192
ISR_NOERR 193
ISR_NOERR 194
ISR_NOERR 195
ISR_NOERR 196
ISR_NOERR 197
ISR_NOERR 198
ISR_NOERR 199
ISR_NOERR 200
ISR_NOERR 201
ISR_NOERR 202
ISR_NOERR 203
ISR_NOERR 204
ISR_NOERR 205
ISR_NOERR 206
ISR_NOERR 207
ISR_NOERR 208
ISR_NOERR 209
ISR_NOERR 210
ISR_NOERR 211
ISR_NOERR 212
ISR_NOERR 213
ISR_NOERR 214
ISR_NOERR 215
ISR_NOERR 216
ISR_NOERR 217
ISR_NOERR 218
ISR_NOERR 219
ISR_NOERR 220
ISR_NOERR 221
ISR_NOERR 222
ISR_NOERR 223
ISR_NOERR 224
ISR_NOERR 225
ISR_NOERR 226
ISR_NOERR 227
ISR_NOERR 228
ISR_NOERR 229
ISR_NOERR 230
ISR_NOERR 231
ISR_NOERR 232
ISR_NOERR 233
ISR_NOERR 234
ISR_NOERR 235
ISR_NOERR 236
ISR_NOERR 237
ISR_NOERR 238
ISR_NOERR 239
ISR_NOERR 240
ISR_NOERR 241
ISR_NOERR 242
ISR_NOERR 243
ISR_NOERR 244
ISR_NOERR 245
ISR_NOERR 246
ISR_NOERR 247
ISR_NOERR 248
ISR_NOERR 249
ISR_NOERR 250
ISR_NOERR 251
ISR_NOERR 252
ISR_NOERR 253
ISR_NOERR 254
ISR_NOERR 255

# ── Common handler ──────────────────────────────
# Saves the full context on the interrupted thread's kernel stack.
# Frame layout (184 bytes, from rsp):
#   +0..112  r15 r14 r13 r12 r11 r10 r9 r8 rbp rdi rsi rdx rcx rbx rax
#   +120     vector
#   +128     errcode
#   +136     RIP
#   +144     CS
#   +152     RFLAGS
#   +160     RSP
#   +168     SS
#   +176     kernel stack top (for TSS.RSP0 in the x86 milestone)
#
# The CPU ALWAYS pushes the full 5-slot frame [SS][RSP][RFLAGS][CS][RIP]
# (plus the error code for vectors that have one) on interrupt entry,
# regardless of privilege (QEMU do_interrupt64 pushes unconditionally, and
# 64-bit IRETQ pops all five). So at entry the stack already holds
# [vector][errcode][RIP][CS][RFLAGS][RSP][SS]; no rebuilding is needed —
# saving the GP regs on top turns those 7 CPU-pushed slots directly into
# the frame's +120..+176 slots, which restore_frame pops exactly.
.align 16
isr_common:
    # Reserve the interrupted function's stack top: the CPU already pushed
    # 7 slots ([SS][RSP][RFLAGS][CS][RIP] + errcode + vector) into its red
    # zone [R0-56, R0); our frame must go BELOW the whole red zone
    # [R0-128, R0), otherwise it clobbers the preempted function's locals
    # (compilers only guarantee the red zone, not [R0-176, R0-128)).
    subq $128, %rsp

    # CPU-pushed 7 slots now sit at rsp+128..rsp+176:
    #   +128 vector, +136 errcode, +144 RIP, +152 CS,
    #   +160 RFLAGS, +168 RSP, +176 SS
    # Rebuild them into the frame's +120..+168 (each pushq reads the next
    # original slot because rsp drops by 8 every time):
    pushq 176(%rsp)               # SS   (+168)
    pushq 176(%rsp)               # RSP  (+160)
    pushq 176(%rsp)               # RFLAGS (+152)
    pushq 176(%rsp)               # CS   (+144)
    pushq 176(%rsp)               # RIP  (+136)
    pushq 176(%rsp)               # errcode (+128)
    pushq 176(%rsp)               # vector (+120)

    # Save GP regs, pushed in restore order: rax at +112 ... r15 at +0
    pushq %rax                    # +112
    pushq %rbx                    # +104
    pushq %rcx                    # +96
    pushq %rdx                    # +88
    pushq %rsi                    # +80
    pushq %rdi                    # +72
    pushq %rbp                    # +64
    pushq %r8                     # +56
    pushq %r9                     # +48
    pushq %r10                    # +40
    pushq %r11                    # +32
    pushq %r12                    # +24
    pushq %r13                    # +16
    pushq %r14                    # +8
    pushq %r15                    # +0

    # kstack_top slot at +176
    movq g_current_kstack_top(%rip), %rax
    movq %rax, 176(%rsp)

    cld

    # rdi = cause (vector), rsi = epc (RIP), rdx = sp, rcx = frame
    movq %rsp, %rcx
    movq 120(%rsp), %rdi
    movq 136(%rsp), %rsi
    movq %rsp, %rdx

    call kr_partix_kernel_interrupt_InterruptBridge_dispatch__JJJJJ

    # Restore from the returned frame (same frame if no context switch)
    movq %rax, %rsp
    jmp  restore_frame

# ── ISR handler pointer table ───────────────────
.section .rodata
.align 8
.globl isr_handlers
isr_handlers:
    .quad isr_0
    .quad isr_1
    .quad isr_2
    .quad isr_3
    .quad isr_4
    .quad isr_5
    .quad isr_6
    .quad isr_7
    .quad isr_8
    .quad isr_9
    .quad isr_10
    .quad isr_11
    .quad isr_12
    .quad isr_13
    .quad isr_14
    .quad isr_15
    .quad isr_16
    .quad isr_17
    .quad isr_18
    .quad isr_19
    .quad isr_20
    .quad isr_21
    .quad isr_22
    .quad isr_23
    .quad isr_24
    .quad isr_25
    .quad isr_26
    .quad isr_27
    .quad isr_28
    .quad isr_29
    .quad isr_30
    .quad isr_31
    .quad isr_32
    .quad isr_33
    .quad isr_34
    .quad isr_35
    .quad isr_36
    .quad isr_37
    .quad isr_38
    .quad isr_39
    .quad isr_40
    .quad isr_41
    .quad isr_42
    .quad isr_43
    .quad isr_44
    .quad isr_45
    .quad isr_46
    .quad isr_47
    .quad isr_48
    .quad isr_49
    .quad isr_50
    .quad isr_51
    .quad isr_52
    .quad isr_53
    .quad isr_54
    .quad isr_55
    .quad isr_56
    .quad isr_57
    .quad isr_58
    .quad isr_59
    .quad isr_60
    .quad isr_61
    .quad isr_62
    .quad isr_63
    .quad isr_64
    .quad isr_65
    .quad isr_66
    .quad isr_67
    .quad isr_68
    .quad isr_69
    .quad isr_70
    .quad isr_71
    .quad isr_72
    .quad isr_73
    .quad isr_74
    .quad isr_75
    .quad isr_76
    .quad isr_77
    .quad isr_78
    .quad isr_79
    .quad isr_80
    .quad isr_81
    .quad isr_82
    .quad isr_83
    .quad isr_84
    .quad isr_85
    .quad isr_86
    .quad isr_87
    .quad isr_88
    .quad isr_89
    .quad isr_90
    .quad isr_91
    .quad isr_92
    .quad isr_93
    .quad isr_94
    .quad isr_95
    .quad isr_96
    .quad isr_97
    .quad isr_98
    .quad isr_99
    .quad isr_100
    .quad isr_101
    .quad isr_102
    .quad isr_103
    .quad isr_104
    .quad isr_105
    .quad isr_106
    .quad isr_107
    .quad isr_108
    .quad isr_109
    .quad isr_110
    .quad isr_111
    .quad isr_112
    .quad isr_113
    .quad isr_114
    .quad isr_115
    .quad isr_116
    .quad isr_117
    .quad isr_118
    .quad isr_119
    .quad isr_120
    .quad isr_121
    .quad isr_122
    .quad isr_123
    .quad isr_124
    .quad isr_125
    .quad isr_126
    .quad isr_127
    .quad isr_128
    .quad isr_129
    .quad isr_130
    .quad isr_131
    .quad isr_132
    .quad isr_133
    .quad isr_134
    .quad isr_135
    .quad isr_136
    .quad isr_137
    .quad isr_138
    .quad isr_139
    .quad isr_140
    .quad isr_141
    .quad isr_142
    .quad isr_143
    .quad isr_144
    .quad isr_145
    .quad isr_146
    .quad isr_147
    .quad isr_148
    .quad isr_149
    .quad isr_150
    .quad isr_151
    .quad isr_152
    .quad isr_153
    .quad isr_154
    .quad isr_155
    .quad isr_156
    .quad isr_157
    .quad isr_158
    .quad isr_159
    .quad isr_160
    .quad isr_161
    .quad isr_162
    .quad isr_163
    .quad isr_164
    .quad isr_165
    .quad isr_166
    .quad isr_167
    .quad isr_168
    .quad isr_169
    .quad isr_170
    .quad isr_171
    .quad isr_172
    .quad isr_173
    .quad isr_174
    .quad isr_175
    .quad isr_176
    .quad isr_177
    .quad isr_178
    .quad isr_179
    .quad isr_180
    .quad isr_181
    .quad isr_182
    .quad isr_183
    .quad isr_184
    .quad isr_185
    .quad isr_186
    .quad isr_187
    .quad isr_188
    .quad isr_189
    .quad isr_190
    .quad isr_191
    .quad isr_192
    .quad isr_193
    .quad isr_194
    .quad isr_195
    .quad isr_196
    .quad isr_197
    .quad isr_198
    .quad isr_199
    .quad isr_200
    .quad isr_201
    .quad isr_202
    .quad isr_203
    .quad isr_204
    .quad isr_205
    .quad isr_206
    .quad isr_207
    .quad isr_208
    .quad isr_209
    .quad isr_210
    .quad isr_211
    .quad isr_212
    .quad isr_213
    .quad isr_214
    .quad isr_215
    .quad isr_216
    .quad isr_217
    .quad isr_218
    .quad isr_219
    .quad isr_220
    .quad isr_221
    .quad isr_222
    .quad isr_223
    .quad isr_224
    .quad isr_225
    .quad isr_226
    .quad isr_227
    .quad isr_228
    .quad isr_229
    .quad isr_230
    .quad isr_231
    .quad isr_232
    .quad isr_233
    .quad isr_234
    .quad isr_235
    .quad isr_236
    .quad isr_237
    .quad isr_238
    .quad isr_239
    .quad isr_240
    .quad isr_241
    .quad isr_242
    .quad isr_243
    .quad isr_244
    .quad isr_245
    .quad isr_246
    .quad isr_247
    .quad isr_248
    .quad isr_249
    .quad isr_250
    .quad isr_251
    .quad isr_252
    .quad isr_253
    .quad isr_254
    .quad isr_255
