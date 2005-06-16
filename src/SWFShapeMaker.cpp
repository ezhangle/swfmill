#include "SWFShapeMaker.h"
#include <stdarg.h>
#include "SWFShapeItem.h"
#include "SWFItem.h"
#include "SWF.h"
#include <math.h>

namespace SWF {

// TODO assure bits is representable in 4 bits!
ShapeMaker::ShapeMaker( List<ShapeItem>* e, double fx, double fy, double ofsx, double ofsy ) {
	edges = e;
	factorx = fx;
	factory = fy;
	offsetx = ofsx;
	offsety = ofsy;
	lastx = lasty = 0;
	diffx = diffy = 0;
}

void ShapeMaker::setupR( int _x, int _y, int fillStyle0, int fillStyle1, int lineStyle ) {
	roundReset();
	int x = roundX(factorx * ( offsetx + _x ) );
	int y = roundY(factory * ( offsety + _y ) );

	diffx = diffy = 0;

	// append shapesetup (whithout styles, this is glyph only for now)
	ShapeSetup *setup = new ShapeSetup;

	setup->setxybits( SWFMaxBitsNeeded( true, 2, x, y ) );
	
	if( fillStyle0 != -1 ) {
		setup->setfillStyle0( fillStyle0 );
		setup->sethasFillStyle0( 1 );
	}
	if( fillStyle1 != -1 ) {
		setup->setfillStyle1( fillStyle1 );
		setup->sethasFillStyle1( 1 );
	}
	if( lineStyle != -1 ) {
		setup->setlineStyle( lineStyle );
		setup->sethasLineStyle( 1 );
	}
	
	setup->sethasMoveTo( 1 );
	setup->setx( x );
	setup->sety( y );
			
	edges->append( setup );

//	fprintf(stderr,"setup %i/%i\n", x, y );
}

void ShapeMaker::lineToR( int _x, int _y ) {
	int x = roundX(factorx * ( offsetx + _x ) );
	int y = roundY(factory * ( offsety + _y ) );

	diffx += x; diffy += y;
	
	SWF::LineTo *segment = new SWF::LineTo;
	segment->setType(1);
	//segment->setbits( maxBitsNeeded( true, 2, x, y ) );
	segment->setx( x );
	segment->sety( y );
	edges->append( segment );
}

void ShapeMaker::curveToR( int _cx, int _cy, int ax, int ay ) {
	int cx = roundX(factorx * ( offsetx + _cx ) );
	int cy = roundY(factory * ( offsety + _cy ) );
	int x = roundX(factorx * ( offsetx + ax ) );
	int y = roundY(factory * ( offsety + ay ) );

	diffx += cx; diffy += cy;
	diffx += x; diffy += y;
	
	CurveTo *segment = new CurveTo;
	segment->setType(2);
	segment->setbits( SWFMaxBitsNeeded( true, 4, x, y, cx, cy ) );
	segment->setx1( cx );
	segment->sety1( cy );
	segment->setx2( x );
	segment->sety2( y );
	edges->append( segment );
}

// cubic to quadratic bezier functions
// thanks to Robert Penner

Point intersectLines( Point p1, Point p2, Point p3, Point p4 ) {
	double x1 = p1.x, y1 = p1.y;
	double x4 = p4.x, y4 = p4.y;
	double dx1 = p2.x-x1;
	double dx2 = p3.x-x4;
	if( dx1 == 0 && dx2 == 0 ) return Point();
	double m1 = (p2.y-y1)/dx1;
	double m2 = (p3.y-y4)/dx2;
	
	if( !dx1 ) {
		return Point( x1, (m2*(x1-x4))+y4 );
	} else if( !dx2 ) {
		return Point( x4, (m1*(x1-x4))+y1 );
	}
	double x = ((-m2 * x4) + y4 + (m1 * x1) - y1) / (m1-m2);
	Point p( x, (m1 * (x-x1)) + y1 );
	fprintf(stderr,"S: %f/%f\n", p.x, p.y );
	return p;
}

Point midLine( const Point& a, const Point& b ) {
	Point p( ((a.x+b.x)/2), ((a.y+b.y)/2) );
	return p;
}

void bezierSplit( const Bezier& b, Bezier* b0, Bezier* b1 ) {
	Point p01 = midLine( b.p0, b.p1 );
	Point p12 = midLine( b.p1, b.p2 );
	Point p23 = midLine( b.p2, b.p3 );
	Point p02 = midLine( p01, p12 );
	Point p13 = midLine( p12, p23 );
	Point p03 = midLine( p02, p13 );
	
	b0->set( b.p0, p01, p02, p03 );
	b1->set( p03, p13, p23, b.p3 );
}

void ShapeMaker::cubicToRec( const Point& a, const Point& b, const Point& c, const Point& d, double k ) {
	Point s = intersectLines( a, b, c, d );
	double dx = (a.x+d.x+s.x*4-(b.x+c.x)*3)*.125;
	double dy = (a.y+d.y+s.y*4-(b.y+c.y)*3)*.125;
	Bezier bz( a, b, c, d );
	Bezier b0, b1;
	if( dx*dx + dy*dy > k ) {
		fprintf(stderr,"split: %f\n",dx*dx + dy*dy);
		bezierSplit( bz, &b0, &b1 );
		// recurse
		cubicToRec( a,    b0.p1, b0.p2, b0.p3, k );
		cubicToRec( b1.p0, b1.p1, b1.p2, d,    k );
	} else {
	//	SWF::LineTo *segment = new SWF::LineTo;
	//	segment->setType(1);
	//	segment->setx( (int)(factorx*d.x) );
	//	segment->sety( (int)(factory*d.y) );
	//	edges->append( segment );
		
	//	lineTo( (int)s.x, (int)s.y );
	//	lineTo( (int)d.x, (int)d.y );
		curveTo( (int)s.x, (int)s.y, (int)d.x, (int)d.y );
	}
}

void ShapeMaker::cubicTo( int x1, int y1, int x2, int y2, int ax, int ay ) {
	Point a(lastx,lasty);
	Point b(x1,y1);
	Point c(x2,y2);
	Point d(ax,ay);

	cubicToRec( a, b, c, d, 50000 );
	//lastx = ax; lasty = ay;
}

void ShapeMaker::close() {
	// diffx/diffy captures rounding errors. they can accumulate a bit! FIXME
	
	if( diffx || diffy ) {
		fprintf(stderr,"WARNING: shape not closed; closing (%i/%i).\n", diffx, diffy);
		fprintf(stderr,"DEBUG: accumulated rounding error (%f/%f).\n", roundx, roundy);
		// closing line
		LineTo *segment = new LineTo;
		segment->setType(1);
		//segment->setbits( maxBitsNeeded( true, 2, x, y ) );
		segment->setx( -diffx );
		segment->sety( -diffy );
		edges->append( segment );
	}

	lastx = lasty = 0;
}

void ShapeMaker::finish() {
	// end shape
	ShapeSetup *setup = new ShapeSetup;
	edges->append( setup );
}

void ShapeMaker::setup( int x, int y, int fillStyle0, int fillStyle1, int lineStyle ) {
	setupR( x-lastx, y-lasty, fillStyle0, fillStyle1, lineStyle );
	lastx = x;
	lasty = y;
}

void ShapeMaker::lineTo( int x, int y ) {
	lineToR( x-lastx, y-lasty );
	lastx = x;
	lasty = y;
}

void ShapeMaker::curveTo( int cx, int cy, int ax, int ay ) {
	curveToR( cx-lastx, cy-lasty, ax-cx, ay-cy );
	lastx = ax;
	lasty = ay;
}

}