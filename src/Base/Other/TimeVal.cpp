#include<hgl/type/DataType.h>

namespace hgl
{
	void SetTimeVal(timeval &tv,const double t_sec)
	{
		tv.tv_sec=t_sec;
		tv.tv_usec=(t_sec-(double)tv.tv_sec)*HGL_MICRO_SEC_PER_SEC;
	}
}//namespace hgl
