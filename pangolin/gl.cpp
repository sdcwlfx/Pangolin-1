#define PANGOLIN_GL_CPP

#include <iostream>
#include <map>
#include <string.h>

#include "platform.h"
#include "gl.h"
#include "gl_internal.h"
#include "simple_math.h"

using namespace std;

namespace pangolin
{
  typedef map<string,PangolinGl*> ContextMap;

  // Map of active contexts
  ContextMap contexts;

  // Context active for current thread
  __thread PangolinGl* context = 0;

  PangolinGl::PangolinGl()
   : quit(false)
  {
  }

  void BindToContext(std::string name)
  {
    ContextMap::iterator ic = contexts.find(name);

    if( ic == contexts.end() )
    {
      // Create and add if not found
      ic = contexts.insert( pair<string,PangolinGl*>(name,new PangolinGl) ).first;
      context = ic->second;
      Display& dc = context->base;
      dc.left = 0;
      dc.bottom = 0;
      dc.top = 1.0;
      dc.right = 1.0;
      dc.aspect = 0;
      dc.handler = &StaticHandler;
    #ifdef HAVE_GLUT
      process::Resize(
        glutGet(GLUT_WINDOW_WIDTH),
        glutGet(GLUT_WINDOW_HEIGHT)
      );
    #else
      process::Resize(640,480);
    #endif //HAVE_GLUT
    }else{
      context = ic->second;
    }
  }

  bool ShouldQuit()
  {
    return context->quit;
  }

  Display* AddDisplay(string name, Attach top, Attach left, Attach bottom, Attach right, bool keep_aspect )
  {
    Display* d = AddDisplay(name,top,left,bottom,right, 0.0f );
    // Aspect as defined by params gets put in d->v.aspect() by above constructor.
    d->aspect = keep_aspect ? d->v.aspect() : 0;
    return d;
  }

  Display* AddDisplay(std::string name, Attach top, Attach left, Attach bottom, Attach right, float aspect )
  {
    Display* d = new Display();
    d->left = left;
    d->top = top;
    d->right = right;
    d->bottom = bottom;
    d->aspect = aspect;
    d->RecomputeViewport(context->base.v);
    context->base[name] = d;
    return d;
  }

  Display*& GetDisplay(string name)
  {
    return context->base[name];
  }


  namespace process
  {
    void Keyboard( unsigned char key, int x, int y)
    {
      //  int mod = glutGetModifiers();

        if( key == GLUT_KEY_TAB)
          glutFullScreenToggle();
        else if( key == GLUT_KEY_ESCAPE)
          context->quit = true;
        else
        {
          context->base.handler->Keyboard(context->base,key,x,y);
        }
    }

    void Mouse( int button, int state, int x, int y)
    {
      context->base.handler->Mouse(context->base,button,state,x,y);
    }

    void MouseMotion( int x, int y)
    {
      context->base.handler->MouseMotion(context->base,x,y);
    }

    void Resize( int width, int height )
    {
      Viewport win(0,0,width,height);
      context->base.RecomputeViewport(win);
    }
  }

#ifdef HAVE_GLUT
  namespace glut
  {

    void CreateWindowAndBind(string window_title, int w, int h)
    {
      if( glutGet(GLUT_INIT_STATE) == 0)
      {
        int argc = 0;
        glutInit(&argc, 0);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
      }
      glutInitWindowSize(w,h);
      glutCreateWindow(window_title.c_str());
      BindToContext(window_title);
      TakeCallbacks();
    }

    void TakeCallbacks()
    {
      glutKeyboardFunc(&process::Keyboard);
      glutReshapeFunc(&process::Resize);
      glutMouseFunc(&process::Mouse);
      glutMotionFunc(&process::MouseMotion);
    }

  }
#endif

  void Viewport::Activate() const
  {
    glViewport(l,b,w,h);
  }

  bool Viewport::Contains(int x, int y) const
  {
    return l <= x && x < (l+w) && b <= y && y < (b+h);
  }


  void OpenGlMatrixSpec::Load() const
  {
    glMatrixMode(type);
    glLoadMatrixd(m);
  }

  void OpenGlRenderState::Apply() const
  {
    for(map<OpenGlMatrixType,OpenGlMatrixSpec>::const_iterator i = stacks.begin(); i != stacks.end(); ++i )
    {
      i->second.Load();
    }
  }


  void Display::RecomputeViewport(const Viewport& p)
  {
    // Compute Bounds based on specification
    v.l = p.l + (left.unit == Pixel ) ? (left.p) : ( left.p * (p.r()-p.l) );
    v.b = p.b + (bottom.unit == Pixel ) ? (bottom.p) : ( bottom.p * (p.t()-p.b) );
    const int r = p.l + (right.unit == Pixel ) ? (right.p) : ( right.p * (p.r()-p.l) );
    const int t = p.b + (top.unit == Pixel ) ? (top.p) : ( top.p * (p.t()-p.b) );
    v.w = r - v.l;
    v.h = t - v.b;

    // Adjust based on aspect requirements
    if( aspect != 0 )
    {
      const float current_aspect = (float)v.w / (float)v.h;
      if( current_aspect < aspect )
      {
        //Adjust height
        const int nh = (int)(v.w / aspect);
        v.b += (v.h-nh)/2;
        v.h = nh;
      }else if( current_aspect > aspect )
      {
        //Adjust width
        const int nw = (int)(v.h * aspect);
        v.l += (v.w-nw)/2;
        v.w = nw;
      }
    }

    // Resize children, if any
    for( map<string,Display*>::const_iterator i = displays.begin(); i != displays.end(); ++i )
    {
      i->second->RecomputeViewport(v);
    }
  }

  void Display::Activate() const
  {
    v.Activate();
  }

  void Display::Activate(const OpenGlRenderState& state ) const
  {
    v.Activate();
    state.Apply();
  }


  Display*& Display::operator[](std::string name)
  {
    return displays[name];
  }

  void Handler::Keyboard(Display& d, unsigned char key, int x, int y)
  {
    // Call childs handler
    for( map<string,Display*>::const_iterator i = d.displays.begin(); i != d.displays.end(); ++i )
    {
      Handler* ch = i->second->handler;
      if( ch && i->second->v.Contains(x,y) ) {
        ch->Keyboard(*(i->second),key,x,y);
        break;
      }
    }
  }

  void Handler::Mouse(Display& d, int button, int state, int x, int y)
  {
    // Call childs handler
    for( map<string,Display*>::const_iterator i = d.displays.begin(); i != d.displays.end(); ++i )
    {
      Handler* ch = i->second->handler;
      if( ch && i->second->v.Contains(x,y) ) {
        ch->Mouse(*(i->second),button,state,x,y);
        break;
      }
    }
  }

  void Handler::MouseMotion(Display& d, int x, int y)
  {
    // Call childs handler
    for( map<string,Display*>::const_iterator i = d.displays.begin(); i != d.displays.end(); ++i )
    {
      Handler* ch = i->second->handler;
      if( ch && i->second->v.Contains(x,y) ) {
        ch->MouseMotion(*(i->second),x,y);
        break;
      }
    }
  }

  void Handler3D::Mouse(Display&, int button, int state, int x, int y)
  {
    if( state == 0 )
    {
      // mouse down
      last_pos[0] = x;
      last_pos[1] = y;

      move_mode = button;
    }else if( state == 1 )
    {
      // mouse up
    }

    cout << state << " " << button << endl;
  }

  void Handler3D::MouseMotion(Display&, int x, int y)
  {
    double T_nc[3*4];
    LieSetIdentity(T_nc);

    if( move_mode == 0 )
    {
      // Rotate
      const double r[] = { (x-last_pos[0])*0.01, (y-last_pos[1])*0.01, 0 };
      double R[3*3];
      MatSkew<>(R,r);
      MatOrtho<3>(R);
      LieSetRotation<>(T_nc,R);
    }else if( move_mode == 1 )
    {
      // in plane translate
      const double t[] = { (x-last_pos[0])*tf, -(y-last_pos[1])*tf, 0};
      LieSetTranslation<>(T_nc,t);
    }

    LieMul4x4bySE3<>(cam_state->stacks[GlProjectionMatrix].m,T_nc,cam_state->stacks[GlProjectionMatrix].m);
  }

  GLdouble& OpenGlMatrixSpec::operator()(int r,int c)
  {
    return m[4*c+r];
  }

  OpenGlMatrixSpec ProjectionMatrix(int w, int h, double fu, double fv, double u0, double v0, double zNear, double zFar )
  {
      // http://www.songho.ca/opengl/gl_projectionmatrix.html
      const double L = +(u0) * zNear / fu;
      const double T = -(h-v0) * zNear / fv;
      const double R = -(w-u0) * zNear / fu;
      const double B = +(v0) * zNear / fv;

      OpenGlMatrixSpec P;
      P.type = GlProjectionMatrix;
      memset(P.m,0,16);
      P(0,0) = 2 * zNear / (R-L);
      P(1,1) = 2 * zNear / (T-B);
      P(2,2) = -(zFar +zNear) / (zFar - zNear);
      P(0,2) = (R+L)/(R-L);
      P(1,2) = (T+B)/(T-B);
      P(3,2) = -1.0;
      P(2,3) =  -(2*zFar*zNear)/(zFar-zNear);
      return P;
  }

  OpenGlMatrixSpec IdentityMatrix(OpenGlMatrixType type)
  {
    OpenGlMatrixSpec P;
    P.type = type;
    memset(P.m,0,16);
    for( int i=0; i<4; ++i ) P(i,i) = 1;
  }



}