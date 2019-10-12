
/*
*
* adapted from https://github.com/stephenyin/h264_sps_decoder
*
* 2019.9.19
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <math.h>

void get_profile(int profile_idc, char* profile_str)
{
    switch(profile_idc){
        case 66:
            strcpy(profile_str, "Baseline");
            break;
        case 77:
            strcpy(profile_str, "Main");
            break;
        case 88:
            strcpy(profile_str, "Extended");
            break;
        case 100:
            strcpy(profile_str, "High(FRExt)");
            break;
        case 110:
            strcpy(profile_str, "High10(FRExt)");
            break;
        case 122:
            strcpy(profile_str, "High4:2:2(FRExt)");
            break;
        case 144:
            strcpy(profile_str, "High4:4:4(FRExt)");
            break;
        default:
            strcpy(profile_str, "Unknown");
    }
}

unsigned int Ue(unsigned char *pBuff, unsigned int nLen, unsigned int *nStartBit)
{
    unsigned int nZeroNum = 0;
    while (*nStartBit < nLen * 8)
    {
        if (pBuff[*nStartBit / 8] & (0x80 >> (*nStartBit % 8)))
        {
            break;
        }
        nZeroNum++;
        *nStartBit+=1;
    }
    *nStartBit+=1;
 
    unsigned long dwRet = 0;
    for (unsigned int i=0; i<nZeroNum; i++)
    {
        dwRet <<= 1;
        if (pBuff[*nStartBit / 8] & (0x80 >> (*nStartBit % 8)))
        {
            dwRet += 1;
        }
        *nStartBit+=1;
    }
    return (1 << nZeroNum) - 1 + dwRet;
}

int Se(unsigned char *pBuff, unsigned int nLen, unsigned int *nStartBit)
{
    int UeVal=Ue(pBuff,nLen,nStartBit);
    double k=UeVal;
    int nValue=ceil(k/2);
    if (UeVal % 2==0)
        nValue=-nValue;
    return nValue;
}

unsigned long u(unsigned int BitCount, unsigned char * buf, unsigned int *nStartBit)
{
    unsigned long dwRet = 0;
    for (unsigned int i=0; i<BitCount; i++)
    {
        dwRet <<= 1;
        if (buf[*nStartBit / 8] & (0x80 >> (*nStartBit % 8)))
        {
            dwRet += 1;
        }
        *nStartBit += 1;
    }
    return dwRet;
}

void de_emulation_prevention(unsigned char* buf,unsigned int* buf_size)
{
    int i=0,j=0;
    unsigned char* tmp_ptr=NULL;
    unsigned int tmp_buf_size=0;
    int val=0;
 
    tmp_ptr=buf;
    tmp_buf_size=*buf_size;
    for(i=0;i<(tmp_buf_size-2);i++)
    {
        //check for 0x000003
        val=(tmp_ptr[i]^0x00) +(tmp_ptr[i+1]^0x00)+(tmp_ptr[i+2]^0x03);
        if(val==0)
        {
            //kick out 0x03
            for(j=i+2;j<tmp_buf_size-1;j++)
                tmp_ptr[j]=tmp_ptr[j+1];
 
            //and so we should devrease bufsize
            (*buf_size)--;
        }
    }
}

//return: 0/false 1/success
int h265_decode_sps(unsigned char * buf,unsigned int nLen,int *width,int *height,int *fps)
{
    unsigned int StartBit=0;
    de_emulation_prevention(buf,&nLen);

    //--- nal_uint_header ---
    int forbidden_zero_bit=u(1,buf,&StartBit);
    int nal_unit_type = u(6,buf,&StartBit);
    if(nal_unit_type != 33)
        return 0;
    int nuh_layer_id = u(6,buf,&StartBit);
    int nuh_temporal_id_plus = u(3,buf,&StartBit);

    //--- seq_parameter_set_rbsp ---
    int sps_video_parameter_set_id = u(4,buf,&StartBit);
    int sps_max_sub_layers_minus1 = u(3,buf,&StartBit);
    int sps_temporal_id_nesting_flag = u(1,buf,&StartBit);
    // printf("sps_video_parameter_set_id/%d\n"
    //     "sps_max_sub_layers_minus1/%d\n"
    //     "sps_temporal_id_nesting_flag/%d\n",
    //     sps_video_parameter_set_id,
    //     sps_max_sub_layers_minus1,
    //     sps_temporal_id_nesting_flag);
    if(sps_temporal_id_nesting_flag) //--- profile_tier_level ---
    {
        int general_profile_space = u(2,buf,&StartBit);
        int general_tier_flag = u(1,buf,&StartBit);
        int general_profile_idc = u(5,buf,&StartBit);
        int general_profile_compatibility_flag[32];
        // printf("general_profile_space/%d\n"
        //     "general_tier_flag/%d\n"
        //     "general_profile_idc/%d\n",
        //     general_profile_space,
        //     general_tier_flag,
        //     general_profile_idc);
        int j;
        for(j = 0; j < 32; j++)
        {
            general_profile_compatibility_flag[j] = u(1,buf,&StartBit);
            // printf("bit[%d]: %d\n", j, general_profile_compatibility_flag[j]);
        }
        int general_progressive_source_flag = u(1,buf,&StartBit);
        int general_interlaced_source_flag = u(1,buf,&StartBit);
        int general_non_packed_constraint_flag = u(1,buf,&StartBit);
        int general_frame_only_constraint_flag = u(1,buf,&StartBit);
        if(general_profile_idc == 4 || general_profile_compatibility_flag[4] ||
            general_profile_idc == 5 || general_profile_compatibility_flag[5] ||
            general_profile_idc == 6 || general_profile_compatibility_flag[6] ||
            general_profile_idc == 7 || general_profile_compatibility_flag[7] ||
            general_profile_idc == 8 || general_profile_compatibility_flag[8] ||
            general_profile_idc == 9 || general_profile_compatibility_flag[9] ||
            general_profile_idc == 10 || general_profile_compatibility_flag[10])
        {
            // printf("> hit 1-1\n");
            int general_max_12bit_constraint_flag = u(1,buf,&StartBit);
            int general_max_10bit_constraint_flag = u(1,buf,&StartBit);
            int general_max_8bit_constraint_flag = u(1,buf,&StartBit);
            int general_max_422chroma_constraint_flag = u(1,buf,&StartBit);
            int general_max_420chroma_constraint_flag = u(1,buf,&StartBit);
            int general_max_monochrome_constraint_flag = u(1,buf,&StartBit);
            int general_intra_constraint_flag = u(1,buf,&StartBit);
            int general_one_picture_only_constraint_flag = u(1,buf,&StartBit);
            int general_lower_bit_rate_constraint_flag = u(1,buf,&StartBit);
            if(general_profile_idc == 5 || general_profile_compatibility_flag[5] ||
                general_profile_idc == 9 || general_profile_compatibility_flag[9] ||
                general_profile_idc == 10 || general_profile_compatibility_flag[10])
            {
                int general_max_14bit_constraint_flag = u(1,buf,&StartBit);
                int general_reserved_zero_33bits = u(33,buf,&StartBit);
            }
            else
            {
                int general_reserved_zero_34bits = u(34,buf,&StartBit);
            }
        }
        else if(general_profile_idc == 2 || general_profile_compatibility_flag[2])
        {
            // printf("> hit 1-2\n");
            int general_reserved_zero_7bits = u(7,buf,&StartBit);
            int general_one_picture_only_constraint_flag = u(1,buf,&StartBit);
            int general_reserved_zero_35bits = u(35,buf,&StartBit);
        }
        else
        {
            // printf("> hit 1-3\n");
            int general_reserved_zero_43bits = u(43,buf,&StartBit);
        }
        if((general_profile_idc >= 1 && general_profile_idc <= 5) ||
            general_profile_idc == 9 ||
            general_profile_compatibility_flag[1] || general_profile_compatibility_flag[2] ||
            general_profile_compatibility_flag[3] || general_profile_compatibility_flag[4] ||
            general_profile_compatibility_flag[5] || general_profile_compatibility_flag[9])
        {
            // printf("> hit 2-1\n");
            int general_inbld_flag = u(1,buf,&StartBit);
        }
        else
        {
            // printf("> hit 2-2\n");
            int general_reserved_zero_bit = u(1,buf,&StartBit);
        }
        int general_level_idc = u(8,buf,&StartBit);
        if(sps_max_sub_layers_minus1 > 0)
        {
            fprintf(stderr, "error: sps_max_sub_layers_minus1 must 0 (%d)\n",
                sps_max_sub_layers_minus1);
            return 0;
        }
    }
    int sps_seq_parameter_set_id = Ue(buf,nLen,&StartBit);
    int chroma_format_idc = Ue(buf,nLen,&StartBit);
    if(chroma_format_idc == 3)
    {
        int separate_colour_plane_flag = u(1,buf,&StartBit);
    }
    int pic_width_in_luma_samples = Ue(buf,nLen,&StartBit);
    int pic_height_in_luma_samples = Ue(buf,nLen,&StartBit);
    int conformance_window_flag = u(1,buf,&StartBit);

    int conf_win_left_offset = 0;
    int conf_win_right_offset = 0;
    int conf_win_top_offset = 0;
    int conf_win_bottom_offset = 0;
    if(conformance_window_flag)
    {
        int conf_win_left_offset = Ue(buf,nLen,&StartBit);
        int conf_win_right_offset = Ue(buf,nLen,&StartBit);
        int conf_win_top_offset = Ue(buf,nLen,&StartBit);
        int conf_win_bottom_offset = Ue(buf,nLen,&StartBit);
    }

    // printf("forbidden_zero_bit/%d,\n"
    // "nal_unit_type/%d, nuh_layer_id/%d,\n"
    // "sps_video_parameter_set_id/%d,\n"
    // "sps_max_sub_layers_minus1/%d,\n"
    // "sps_temporal_id_nesting_flag/%d\n"
    // "sps_seq_parameter_set_id/%d\n"
    // "chroma_format_idc/%d\n",
    //     forbidden_zero_bit,
    //     nal_unit_type,
    //     nuh_layer_id,
    //     sps_video_parameter_set_id,
    //     sps_max_sub_layers_minus1,
    //     sps_temporal_id_nesting_flag,
    //     sps_seq_parameter_set_id,
    //     chroma_format_idc);

    *width = pic_width_in_luma_samples - (conf_win_left_offset + conf_win_right_offset);
    *height = pic_height_in_luma_samples - (conf_win_top_offset + conf_win_bottom_offset);
    *fps = 0;

    return 1;
}

//return: 0/false 1/success
int h264_decode_sps(unsigned char * buf,unsigned int nLen,int *width,int *height,int *fps)
{
    unsigned int StartBit=0;
    de_emulation_prevention(buf,&nLen);
 
    int timing_info_present_flag = 0;
    int forbidden_zero_bit=u(1,buf,&StartBit);
    int nal_ref_idc=u(2,buf,&StartBit);
    int nal_unit_type=u(5,buf,&StartBit);
    if(nal_unit_type==7)
    {
        int profile_idc=u(8,buf,&StartBit);
        int constraint_set0_flag=u(1,buf,&StartBit);//(buf[1] & 0x80)>>7;
        int constraint_set1_flag=u(1,buf,&StartBit);//(buf[1] & 0x40)>>6;
        int constraint_set2_flag=u(1,buf,&StartBit);//(buf[1] & 0x20)>>5;
        int constraint_set3_flag=u(1,buf,&StartBit);//(buf[1] & 0x10)>>4;
        int reserved_zero_4bits=u(4,buf,&StartBit);
        int level_idc=u(8,buf,&StartBit);
 
        int seq_parameter_set_id=Ue(buf,nLen,&StartBit);
 
        int chroma_format_idc = 1;
        // if( profile_idc == 100 || profile_idc == 110 ||
        //     profile_idc == 122 || profile_idc == 144 )
        if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
            profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
            profile_idc == 86 || profile_idc == 118 || profile_idc == 128 || 
            profile_idc == 144 || profile_idc == 138 || profile_idc == 139 || 
            profile_idc == 134 || profile_idc == 135)
        {
            chroma_format_idc=Ue(buf,nLen,&StartBit);
            if( chroma_format_idc == 3 )
            {
                unsigned long residual_colour_transform_flag=u(1,buf,&StartBit);
            }
            int bit_depth_luma_minus8=Ue(buf,nLen,&StartBit);
            int bit_depth_chroma_minus8=Ue(buf,nLen,&StartBit);
            int qpprime_y_zero_transform_bypass_flag=u(1,buf,&StartBit);
            int seq_scaling_matrix_present_flag=u(1,buf,&StartBit);
 
            int seq_scaling_list_present_flag[8];
            if( seq_scaling_matrix_present_flag )
            {
                for( int i = 0; i < 8; i++ ) {
                    seq_scaling_list_present_flag[i]=u(1,buf,&StartBit);
                }
            }
        }
        int log2_max_frame_num_minus4=Ue(buf,nLen,&StartBit);
        int pic_order_cnt_type=Ue(buf,nLen,&StartBit);
        if( pic_order_cnt_type == 0 )
        {
            unsigned int log2_max_pic_order_cnt_lsb_minus4=Ue(buf,nLen,&StartBit);
        }
        else if( pic_order_cnt_type == 1 )
        {
            int delta_pic_order_always_zero_flag=u(1,buf,&StartBit);
            int offset_for_non_ref_pic=Se(buf,nLen,&StartBit);
            int offset_for_top_to_bottom_field=Se(buf,nLen,&StartBit);
            int num_ref_frames_in_pic_order_cnt_cycle=Ue(buf,nLen,&StartBit);
 
            int *offset_for_ref_frame = (int*)calloc(num_ref_frames_in_pic_order_cnt_cycle, sizeof(int));
            for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
                offset_for_ref_frame[i]=Se(buf,nLen,&StartBit);
            free(offset_for_ref_frame);
        }
        int num_ref_frames=Ue(buf,nLen,&StartBit);
        int gaps_in_frame_num_value_allowed_flag=u(1,buf,&StartBit);
        int pic_width_in_mbs_minus1=Ue(buf,nLen,&StartBit);
        int pic_height_in_map_units_minus1=Ue(buf,nLen,&StartBit);
        

        int frame_mbs_only_flag=u(1,buf,&StartBit);
        if(!frame_mbs_only_flag)
        {
            unsigned long mb_adaptive_frame_field_flag=u(1,buf,&StartBit);
        }
        int direct_8x8_inference_flag=u(1,buf,&StartBit);
        int frame_cropping_flag=u(1,buf,&StartBit);

        int frame_crop_left_offset=0;
        int frame_crop_right_offset=0;
        int frame_crop_top_offset=0;
        int frame_crop_bottom_offset=0;
        if(frame_cropping_flag)
        {
            frame_crop_left_offset=Ue(buf,nLen,&StartBit);
            frame_crop_right_offset=Ue(buf,nLen,&StartBit);
            frame_crop_top_offset=Ue(buf,nLen,&StartBit);
            frame_crop_bottom_offset=Ue(buf,nLen,&StartBit);
        }
        int vui_parameter_present_flag=u(1,buf,&StartBit);
        if(vui_parameter_present_flag)
        {
            int aspect_ratio_info_present_flag=u(1,buf,&StartBit);
            if(aspect_ratio_info_present_flag)
            {
                int aspect_ratio_idc=u(8,buf,&StartBit);
                if(aspect_ratio_idc==255)
                {
                    int sar_width=u(16,buf,&StartBit);
                    int sar_height=u(16,buf,&StartBit);
                }
            }
            int overscan_info_present_flag=u(1,buf,&StartBit);
            if(overscan_info_present_flag)
            {
                int overscan_appropriate_flagu=u(1,buf,&StartBit);
            }
            int video_signal_type_present_flag=u(1,buf,&StartBit);
            if(video_signal_type_present_flag)
            {
                int video_format=u(3,buf,&StartBit);
                int video_full_range_flag=u(1,buf,&StartBit);
                int colour_description_present_flag=u(1,buf,&StartBit);
                if(colour_description_present_flag)
                {
                    int colour_primaries=u(8,buf,&StartBit);
                    int transfer_characteristics=u(8,buf,&StartBit);
                    int matrix_coefficients=u(8,buf,&StartBit);
                }
            }
            int chroma_loc_info_present_flag=u(1,buf,&StartBit);
            if(chroma_loc_info_present_flag)
            {
                int chroma_sample_loc_type_top_field=Ue(buf,nLen,&StartBit);
                int chroma_sample_loc_type_bottom_field=Ue(buf,nLen,&StartBit);
            }
            timing_info_present_flag=u(1,buf,&StartBit);
 
            if(timing_info_present_flag)
            {
                int num_units_in_tick=u(32,buf,&StartBit);
                int time_scale=u(32,buf,&StartBit);
                *fps=time_scale/num_units_in_tick;
                int fixed_frame_rate_flag=u(1,buf,&StartBit);
                if(fixed_frame_rate_flag)
                {
                    *fps = (*fps)/2;
                }
            }
        }

        //Source, decoded, and output picture formats
        int crop_unit_x = 1;
        int crop_unit_y = 2 - frame_mbs_only_flag;      //monochrome or 4:4:4
        if (chroma_format_idc == 1) {   //4:2:0
            crop_unit_x = 2;
            crop_unit_y = 2 * (2 - frame_mbs_only_flag);
        }else if (chroma_format_idc == 2) {    //4:2:2
            crop_unit_x = 2;
            crop_unit_y = 2 - frame_mbs_only_flag;
        }

        *width=(pic_width_in_mbs_minus1+1)*16;
        *height=(2-frame_mbs_only_flag)*(pic_height_in_map_units_minus1+1)*16;
    
        *width-=crop_unit_x*(frame_crop_left_offset+frame_crop_right_offset);
        *height-=crop_unit_y*(frame_crop_top_offset+frame_crop_bottom_offset);

        *fps = 0;

        // char profile_str[32] = {0};
        // get_profile(profile_idc, &profile_str[0]);
        // if(timing_info_present_flag){
        //     printf("H.264 SPS: -> video size %dx%d, %d fps, profile(%d) %s\n",
        //         *width, *height, *fps, profile_idc, profile_str);
        // } else {
        //     printf("H.264 SPS: -> video size %dx%d, unknown fps, profile(%d) %s\n",
        //         *width, *height, profile_idc, profile_str);
        // }
        return 1;
    }
    else
        return 0;
}

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

//return: 0/false 1/success
int h26x_get_width_height(char *filePath, int *width, int *height, char isH264)
{
    unsigned char buff[1024] = {0};
    int fps, fd, ret;
    int retFinal = 0;
    if((fd = open(filePath, O_RDONLY)) < 1)
    {
        fprintf(stderr, "h26x_get_width_height: open %s err !\n", filePath);
        return retFinal;
    }
    if((ret = read(fd, buff, 1024)) > 0)
    {
        int i, step = 0;
        if(isH264)
        {
            for(i = 0; i < ret - 10; i++)
            {
                if(buff[i] == 0 && buff[i+1] == 0 && buff[i+2] == 0 && buff[i+3] == 1)
                {
                    if(step == 0 && (buff[i+4]&0x1F) == 7)
                    {
                        i += 4;
                        step = i;
                    }
                    else if(step)
                    {
                        retFinal = h264_decode_sps(&buff[step], i-step, width, height, &fps);
                        break;
                    }
                }
            }
        }
        else
        {
            for(i = 0; i < ret - 10; i++)
            {
                if(buff[i] == 0 && buff[i+1] == 0 && buff[i+2] == 0 && buff[i+3] == 1)
                {
                    if(step == 0 && ((buff[i+4]&0x7E)>>1) == 33)
                    {
                        i += 4;
                        step = i;
                    }
                    else if(step)
                    {
                        retFinal = h265_decode_sps(&buff[step], i-step, width, height, &fps);
                        break;
                    }
                }
            }
        }
    }
    close(fd);
    return retFinal;
}

//return: 0/false 1/success
int mp4_get_width_height(char *filePath, int *width, int *height)
{
    unsigned char buff[9] = {0};
    int fd, ret;
    unsigned int size;
    char *type;
    int retFinal = 0;

    type = (char*)&buff[4];

    if((fd = open(filePath, O_RDONLY)) < 1)
    {
        fprintf(stderr, "mp4_get_width_height: open %s err !\n", filePath);
        return retFinal;
    }
    while((ret = read(fd, buff, 8)) > 0)
    {
        size = (buff[0]<<24)|(buff[1]<<16)|(buff[2]<<8)|buff[3];
        // printf("  type: %s  size: %d\n", type, size);

        if(strncmp(type, "tkhd", 4) == 0)
        {
            //取该box的后8字节,宽高信息就在其中
            if(lseek(fd, size - ret - 8, SEEK_CUR) < 1)
            {
                fprintf(stderr, "mp4_get_width_height: lseek err\n");
                break;
            }
            if((ret = read(fd, buff, 8)) == 8)
            {
                // [16.16]格式??????
                if(width)
                    *width = (buff[0]<<8)|buff[1];
                if(height)
                    *height = (buff[4]<<8)|buff[5];
                
                retFinal = 1;
            }
            break;
        }
        else if(strncmp(type, "moov", 4) == 0 || strncmp(type, "trak", 4) == 0)//tkhd的上级目录,直接移动8字节即可
            size = ret;
        else if(size == 1 && strncmp(type, "mdat", 4) == 0)//mdat的largesize情况,要读取后面8字节作为box的size
        {
            if(read(fd, buff, 8) != 8)
            {
                fprintf(stderr, "mp4_get_width_height: read err\n");
                break;
            }
            //计算当前的mdat包还剩多少字节
            int i;
            unsigned long long lsize = 0;
            for(i = 0, lsize = 0; i < 8; i++)
            {
                lsize <<= 8;
                lsize |= buff[i];
            }
            // printf("  type: %s  largesize: %ld\n", type, lsize);
            lsize = lsize - ret - 8;
            //lseek传参是int型,需小块移动
            unsigned int tsize = 0;
            while(lsize > 0)
            {
                if(lsize > 0x7FFFFFFF)
                    tsize = 0x7FFFFFFF;
                else
                    tsize = (unsigned int)lsize;
                lsize -= tsize;
                if(lseek(fd, tsize, SEEK_CUR) < 0)
                    break;
            }
            //不再移动
            size = ret;
        }
        
        if(size < ret)
        {
            fprintf(stderr, "mp4_get_width_height: format err at addr: %ld, size = %d\n", 
                lseek(fd, 0, SEEK_CUR) - ret, size);
            break;
        }
        else if(lseek(fd, size - ret, SEEK_CUR) < 0)
        {
            fprintf(stderr, "mp4_get_width_height: lseek to final\n");
            break;
        }
    }
    close(fd);
    return retFinal;
}


static int mp4_fd = 0;
static unsigned long long mdat_size = 0;

void mp4_close(void)
{
    if(mp4_fd > 0)
        close(mp4_fd);
    mp4_fd = 0;
    mdat_size = 0;
}

void mp4_open(char *filePath)
{
    unsigned char buff[9] = {0};
    char *type;
    int ret;

    type = (char*)&buff[4];

    if(mp4_fd)
        mp4_close();
    mp4_fd = open(filePath, O_RDONLY);
    if(mp4_fd > 0)
    {
        //找到mdat
        while((ret = read(mp4_fd, buff, 8)) > 0)
        {
            mdat_size = (buff[0]<<24)|(buff[1]<<16)|(buff[2]<<8)|buff[3];

            if(strncmp(type, "mdat", 4) == 0)
            {
                if(mdat_size == 1)//largesize模式取后面8字节作为size
                {
                    if(read(mp4_fd, buff, 8) != 8)
                    {
                        fprintf(stderr, "mp4_open: read err\n");
                        mp4_close();
                        break;
                    }
                    int i;
                    for(i = 0, mdat_size = 0; i < 8; i++)
                    {
                        mdat_size <<= 8;
                        mdat_size |= buff[i];
                    }
                    mdat_size = mdat_size - ret - 8;
                }
                break;
            }
            
            if(lseek(mp4_fd, mdat_size - ret, SEEK_CUR) < 0)
            {
                fprintf(stderr, "mp4_open: lseek to final\n");
                mp4_close();
                break;
            }
        }
    }
}

//return: <=0 final or error
int mp4_read_frame(unsigned char *data, int dataMaxLen)
{
    if(mp4_fd < 1 || mdat_size < 1)
    {
        mp4_close();
        return -1;
    }
    
    unsigned char buff[9] = {0};
    unsigned int size = 0;
    int retFinal = 0, ret;

    if(read(mp4_fd, buff, 4) == 4)
    {
        size = (buff[0]<<24)|(buff[1]<<16)|(buff[2]<<8)|buff[3];
        if(size < 1)
            goto error_return;
        
        // printf("mp4_read_frame: size %d / remain %ld\n", size, mdat_size);

        mdat_size = mdat_size - 4 - size;

        if(size > dataMaxLen)
        {
            retFinal = read(mp4_fd, data, dataMaxLen);
            read(mp4_fd, buff, size - dataMaxLen);
        }
        else
            retFinal = read(mp4_fd, data, size);
        
        if(retFinal < 1)
            goto error_return;

        //寻找下一个mdat
        if(mdat_size == 0)
        {
            char *type;
            type = (char*)&buff[4];

            //找到mdat
            while((ret = read(mp4_fd, buff, 8)) > 0)
            {
                mdat_size = (buff[0]<<24)|(buff[1]<<16)|(buff[2]<<8)|buff[3];

                if(strncmp(type, "mdat", 4) == 0)
                {
                    if(mdat_size == 1)
                    {
                        if(read(mp4_fd, buff, 8) != 8)
                        {
                            fprintf(stderr, "mp4_open: read err\n");
                            mp4_close();
                            break;
                        }
                        int i;
                        for(i = 0, mdat_size = 0; i < 8; i++)
                        {
                            mdat_size <<= 8;
                            mdat_size |= buff[i];
                        }
                        mdat_size = mdat_size - ret - 8;
                    }
                    break;
                }
                
                if(lseek(mp4_fd, mdat_size - ret, SEEK_CUR) < 0)
                {
                    fprintf(stderr, "mp4_open: lseek to final\n");
                    mp4_close();
                    break;
                }
            }
        }

        return retFinal;
    }

error_return:
    //error
    mp4_close();
    return 0;
}
