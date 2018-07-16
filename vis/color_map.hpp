#ifndef COLOR_MAP_HPP
#define COLOR_MAP_HPP
#include <vector>
#include <fstream>
#include <iterator>
#include "tipl/utility/basic_image.hpp"
#include "tipl/numerical/basic_op.hpp"
namespace tipl{

inline unsigned char color_spectrum_value(unsigned char center, unsigned char value)
{
    unsigned char dif = center > value ? center-value:value-center;
    if(dif < 32)
        return 255;
    dif -= 32;
    if(dif >= 64)
        return 0;
    return 255-(dif << 2);
}

struct color_bar : public tipl::color_image{
public:
    color_bar(unsigned int width,unsigned int height)
    {
        resize(tipl::geometry<2>(width,height));
    }
    void two_color(tipl::rgb from_color,tipl::rgb to_color)
    {
        resize(tipl::geometry<2>(20,256));
        for(int index = 1;index < height();++index)
        {
            float findex = (float)index/(float)height();
            tipl::rgb color;
            for(unsigned char rgb_index = 0;rgb_index < 3;++rgb_index)
                color[rgb_index] = (unsigned char)((float)from_color[rgb_index]*findex+(float)to_color[rgb_index]*(1.0-findex));
            std::fill(begin()+index*width()+1,begin()+(index+1)*width()-1,color);
        }
    }
    void spectrum(void)
    {
        for(int index = 1;index < height();++index)
        {
            unsigned char findex = (unsigned char)((float)index*255.0f/height());
            tipl::rgb color;
            color.r = tipl::color_spectrum_value(64,findex);
            color.g = tipl::color_spectrum_value(128,findex);
            color.b = tipl::color_spectrum_value(128+64,findex);
            std::fill(begin()+index*width()+1,begin()+(index+1)*width()-1,color);
        }
    }
};

struct color_map{
    std::vector<tipl::vector<3,float> > color;
public:
    color_map(void):color(256){}
    size_t size(void)const{return color.size();}
    const tipl::vector<3,float>& operator[](unsigned int index) const{return color[255,index];}
    tipl::vector<3,float> min_color(void)const{return color.front();}
    tipl::vector<3,float> max_color(void)const{return color.back();}
    void two_color(tipl::rgb from_color,tipl::rgb to_color)
    {
        color.resize(256);
        for(unsigned int index = 0;index < 256;++index)
        {
            float findex = (float)index/255.0f;
            for(unsigned char rgb_index = 0;rgb_index < 3;++rgb_index)
                color[index][rgb_index] = ((float)to_color[rgb_index]*findex+(float)from_color[rgb_index]*(1.0-findex))/255.0f;
        }
    }
    void spectrum(void)
    {
        color.resize(256);
        for(unsigned int index = 0;index < 256;++index)
        {
            color[index][0] = (float)tipl::color_spectrum_value(128+64,index)/255.0f;
            color[index][1] = (float)tipl::color_spectrum_value(128,index)/255.0f;
            color[index][2] = (float)tipl::color_spectrum_value(64,index)/255.0f;
        }
    }
};


struct color_map_rgb{
    std::vector<tipl::rgb> color;
public:
    color_map_rgb(void):color(256){}
    size_t size(void)const{return color.size();}
    const tipl::rgb& operator[](unsigned int index) const{return color[index];}
    tipl::rgb min_color(void)const{return color.front();}
    tipl::rgb max_color(void)const{return color.back();}
    void two_color(tipl::rgb from_color,tipl::rgb to_color)
    {
        for(unsigned int index = 0;index < 256;++index)
        {
            for(unsigned char rgb_index = 0;rgb_index < 3;++rgb_index)
                color[index][rgb_index] =
                        (unsigned char)(std::min<short>(255,((float)to_color[rgb_index]*index+(float)from_color[rgb_index]*(255-index))/255.0f));
        }
    }
    void spectrum(void)
    {
        for(unsigned int index = 0;index < 256;++index)
        {
            color[index][2] = tipl::color_spectrum_value(128+64,index);
            color[index][1] = tipl::color_spectrum_value(128,index);
            color[index][0] = tipl::color_spectrum_value(64,index);
        }
    }
    bool load_from_file(const char* file_name)
    {
        std::ifstream in(file_name);
        if(!in)
            return false;
        std::vector<float> values;
        std::copy(std::istream_iterator<float>(in),
                  std::istream_iterator<float>(),std::back_inserter(values));
        float max_value = *std::max_element(values.begin(),values.end());
        if(max_value < 2.0 && max_value != 0.0)
        {
            for(unsigned int i = 0;i < values.size();++i)
                values[i] = float(std::max<int>(0,std::min<int>(255,int(std::floor(values[i]*256.0f/max_value)))));
        }
        if(values.size() < 3)
            return false;
        color.clear();
        for(unsigned int i = 2;i < values.size();i += 3)
            color.push_back(tipl::rgb((unsigned char)values[i-2],
                                             (unsigned char)values[i-1],
                                             (unsigned char)values[i]));
        return true;
    }
};

template<class value_type>
struct value_to_color{
private:
    value_type min_value,max_value,r;
    tipl::color_map_rgb map;
public:
    value_to_color(void):min_value(0),r(1){}
    tipl::rgb min_color(void)const{return map.min_color();}
    tipl::rgb max_color(void)const{return map.max_color();}

    void set_range(value_type min_value_,value_type max_value_)
    {
        min_value = min_value_;
        max_value = max_value_;
        max_value_ -= min_value_;
        r = (max_value_ == 0.0) ? 1.0:(float)map.size()/max_value_;
    }
    void set_color_map(const tipl::color_map_rgb& rhs)
    {
        map = rhs;
        r = max_value-min_value;
        r = (r == 0.0) ? 1.0:(float)map.size()/r;
    }
    void two_color(tipl::rgb from_color,tipl::rgb to_color)
    {
        map.two_color(from_color,to_color);
    }

    const tipl::rgb& operator[](value_type value)const
    {
        value -= min_value;
        value *= r;
        int ivalue = std::floor(value);
        if(ivalue < 0)
            ivalue = 0;
        if(ivalue >= map.size())
            ivalue = map.size()-1;
        return map[ivalue];
    }
    template<class value_image_type,class color_image_type>
    void convert(const value_image_type& I1,color_image_type& I2) const
    {
        I2.resize(I1.geometry());
        for(unsigned int i = 0;i < I1.size();++i)
            I2[i] = (*this)[I1[i]];
    }
};

}



#endif//COLOR_MAP_HPP

 
