#ifndef INCLUDED_BOUNDED_ARRAY_H
#define INCLUDED_BOUNDED_ARRAY_H


// ****************************************************************************
// Class BoundedArray
// ****************************************************************************

template <class T>
class BoundedArray
{
protected:
	T				*m_data;
	unsigned int	m_numElements;

public:
	BoundedArray    ();
	BoundedArray    (BoundedArray &_otherArray);
	BoundedArray    (unsigned int _numElements);
	~BoundedArray   ();

    void Empty      ();
		
	void Initialise (unsigned int _numElements);             // Only need to call Initialise if you used the default constructor
    void Initialise (BoundedArray &_otherArray);


    inline T        &operator [] (unsigned int _index);
    inline const T  &operator [] (unsigned int _index) const;

    inline unsigned int Size() const;
	
	void SetAll(T const &_value);
};


#include "bounded_array.cpp"


#endif
