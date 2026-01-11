import turtle as t

s = 100

t.colormode(255)
t.tracer(0,0)
t.penup()
t.left(90)
t.forward(2*s+2)
t.left(90)
t.forward(2*s)
t.right(180)
for row in range(4):
    for column in range(4):
        r = (row*4+column) * 17;
        g = r
        b = r    
        t.fillcolor(r,g,b)
        t.begin_fill()
        t.forward(s)
        t.right(90)
        t.forward(s)
        t.right(90)
        t.forward(s)
        t.right(90)
        t.forward(s)
        t.right(90)
        t.end_fill()
        t.forward(s)
    t.right(90)
    t.forward(s)
    t.right(90)
    t.forward(4*s)
    t.right(180)

t.mainloop()