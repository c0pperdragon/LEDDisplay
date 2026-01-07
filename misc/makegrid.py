import math

vertices=[]

def exportascii(filename):
    f = open(filename, "w")    
    f.write("solid unnamed\n")
    for i in range(0,len(vertices),12):
        f.write(" facet normal {0} {1} {2}\n".format(vertices[i],vertices[i+1],vertices[i+2]))
        f.write("  outer loop\n")
        f.write("   vertex {0} {1} {2}\n".format(vertices[i+3],vertices[i+4],vertices[i+5]))
        f.write("   vertex {0} {1} {2}\n".format(vertices[i+6],vertices[i+7],vertices[i+8]))
        f.write("   vertex {0} {1} {2}\n".format(vertices[i+9],vertices[i+10],vertices[i+11]))
        f.write("  endloop\n")
        f.write(" endfacet\n")
    f.write("endsolid unnamed\n")
    f.close()    
    
def triangle(ax,ay,az, bx,by,bz, cx,cy,cz):
    u1 = cx-bx
    u2 = cy-by
    u3 = cz-bz
    v1 = ax-bx
    v2 = ay-by
    v3 = az-bz
    nx = u2*v3-u3*v2
    ny = u3*v1-u1*v3
    nz = u1*v2-u2*v1
    l = math.sqrt(nx*nx+ny*ny+nz*nz)
    vertices.extend([nx/l,ny/l,nz/l, ax,ay,az, bx,by,bz, cx,cy,cz])        

def rectangle(ax,ay,az, bx,by,bz, cx,cy,cz):
    mx = (ax+cx)/2
    my = (ay+cy)/2
    mz = (az+cz)/2
    triangle(ax,ay,az, bx,by,bz, cx,cy,cz)
    triangle(cx,cy,cz, mx+mx-bx, my+my-by, mz+mz-bz, ax,ay,az)

def rectanglepair(ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz):
    rectangle(ax,ay,az, bx,by,bz, cx,cy,cz);
    rectangle(cx+(dx-ax),cy+(dy-ay),cz+(dz-az), bx+(dx-ax),by+(dy-ay),bz+(dz-az), dx,dy,dz)  

def patch(ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz):
    triangle(ax,ay,az, bx,by,bz, cx,cy,cz)
    triangle(ax,ay,az, cx,cy,cz, dx,dy,dz)
    
def ridge(ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz, ex,ey,ez, fx,fy,fz):
    patch(ax,ay,az, bx,by,bz, ex,ey,ez, dx,dy,dz)
    patch(ex,ey,ez, bx,by,bz, cx,cy,cz, fx,fy,fz)

def fan(mx,my,mz, vertices):
    prev = vertices[-1]
    for v in vertices:
        triangle(prev[0],prev[1],prev[2], v[0],v[1],v[2], mx,my,mz)
        prev=v

def cylinder(bottom,top):
    prevb = bottom[-1]
    prevt = top[-1]
    for b, t in zip(bottom, top):
        triangle(prevt[0],prevt[1],prevt[2], t[0],t[1],t[2], b[0],b[1],b[2])
        triangle(b[0],b[1],b[2], prevb[0],prevb[1],prevb[2], prevt[0],prevt[1],prevt[2])
        prevb = b
        prevt = t
    
def box_ortho(ax,ay,az, bx,by,bz):
    rectanglepair(ax,ay,az, ax,by,az, bx,by,az, ax,ay,bz)
    rectanglepair(ax,ay,az, ax,ay,bz, ax,by,bz, bx,ay,az)
    rectanglepair(ax,ay,az, bx,ay,az, bx,ay,bz, ax,by,az)
    
def stem(x,y,thickness,rasterheight,stemheight,stemthickness):
    t = thickness
    rh=rasterheight
    sh = stemheight    
    s = stemthickness
    bottom = [[x,y-t,rh],[x+t/2,y-t/2,rh],[x+t,y,rh],[x+t/2,y+t/2,rh],[x,y+t,rh],[x-t/2,y+t/2,rh],[x-t,y,rh],[x-t/2,y-t/2,rh]]
    top = [[x,y-s,sh],[x+s/2,y-s/2,sh],[x+s,y,sh],[x+s/2,y+s/2,sh],[x,y+s,sh],[x-s/2,y+s/2,sh],[x-s,y,sh],[x-s/2,y-s/2,sh]]
    cylinder(bottom,top)
    top.reverse()
    fan(x,y,sh, top)

def grid(x1,y1, rows,columns, cellwidth,thickness,rasterheight,stemheight,stemthickness):
    t = thickness
    rh=rasterheight
    cw=cellwidth
    x2 = x1+cellwidth*columns
    y2 = y1+cellwidth*rows
    for r in range(rows):
        for c in range(columns):
            x = x1+c*cellwidth
            y = y1+r*cellwidth
            if c==0:
                 ridge(x,y,0, x,y+t/2,0, x,y+t,0, x,y,rasterheight, x-t/2,y+t/2,rasterheight, x,y+t,rasterheight)
                 ridge(x2,y,0, x2,y+t/2,0, x2,y+t,0, x2,y,rasterheight, x2-t/2,y+t/2,rasterheight, x2,y+t,rasterheight)
            if r==0:
                 ridge(x,y,0, x+t/2,y,0, x+t,y,0, x,y,rasterheight, x+t/2,y-t/2,rasterheight, x+t,y,rasterheight)
                 ridge(x,y2,0, x+t/2,y2,0, x+t,y2,0, x,y2,rasterheight, x+t/2,y2-t/2,rasterheight, x+t,y2,rasterheight)

            stem(x+thickness/2,y+thickness/2, thickness,rasterheight,stemheight,stemthickness)
            fan(x+cw/2,y+t/2,rh, [[x+t,y,rh],[x+t+t/2,y+t/2,rh],[x+t,y+t,rh],[x+cw,y+t,rh],[x+cw-t/2,y+t/2,rh],[x+cw,y,rh]])
            fan(x+t/2,y+cw/2,rh, [[x,y+t,rh],[x,y+cw,rh],[x+t/2,y+cw-t/2,rh],[x+t,y+cw,rh],[x+t,y+t,rh],[x+t/2,y+t+t/2,rh]])
            rectangle(x+thickness,y,0, x+cellwidth,y,0, x+cellwidth,y+thickness,0)
            rectangle(x,y+thickness,0, x+thickness,y+thickness,0, x+thickness,y+cellwidth,0)                     
            rectanglepair(x+thickness,y,0, x+thickness,y,rasterheight, x+cellwidth,y,rasterheight, x+thickness,y+thickness,0)
            rectanglepair(x,y+thickness,0, x,y+cellwidth,0, x,y+cellwidth,rasterheight, x+thickness,y+thickness,0) 
            rectangle(x,y,0, x+thickness,y,0, x+thickness,y+thickness,0)            
            

grid(0,0,    5,5, 3,0.90, 3,4.5, 1.05)
grid(25,0,   5,6, 3,0.90, 3,4.5, 1.10)
grid(50,0,   5,7, 3,0.90, 3,4.5, 1.15)
grid(0,25,   5,8, 3,0.90, 3,4.5, 1.20)
grid(50,25,  6,6, 3,0.90, 3,4.5, 1.25)
grid(0,50,   6,7, 3,0.80, 3,4.5, 1.05)
grid(25,50,  6,8, 3,0.80, 3,4.5, 1.10)

exportascii("box.stl")
