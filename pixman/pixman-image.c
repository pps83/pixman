/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>

#include "pixman.h"
#include "pixman-private.h"

enum
{
    PIXMAN_BAD_VALUE,
    PIXMAN_BAD_ALLOC
};

static void
init_common (image_common_t *common)
{
    common->transform = NULL;
    common->clip_region = NULL;
    common->repeat = PIXMAN_REPEAT_NONE;
    common->filter = PIXMAN_FILTER_NEAREST;
    common->filter_params = NULL;
    common->n_filter_params = 0;
    common->alpha_map = NULL;
    common->component_alpha = FALSE;
}

static void
init_source_image (source_image_t *image)
{
    init_common (&image->common);
    image->class = SOURCE_IMAGE_CLASS_UNKNOWN;
}

static void
init_gradient (gradient_t     *gradient,
	       int	       stop_count,
	       pixman_fixed_t *stop_points,
	       pixman_color_t *stop_colors,
	       int            *error)
{
    int i;
    pixman_fixed_t dpos;

    if (stop_count <= 0)
    {
        *error = PIXMAN_BAD_VALUE;
        return;
    }

    init_source_image (&gradient->common);
    
    dpos = -1;
    for (i = 0; i < stop_count; ++i)
    {
        if (stop_points[i] < dpos || stop_points[i] > (1<<16))
	{
            *error = PIXMAN_BAD_VALUE;
            return;
        }
        dpos = stop_points[i];
    }

    gradient->stops = malloc (stop_count * sizeof (gradient_stop_t));
    if (!gradient->stops)
    {
        *error = PIXMAN_BAD_ALLOC;
        return;
    }

    gradient->n_stops = stop_count;

    for (i = 0; i < stop_count; ++i)
    {
        gradient->stops[i].x = stop_points[i];
        gradient->stops[i].color = stop_colors[i];
    }

    gradient->stop_range = 0xffff;
    gradient->color_table = NULL;
    gradient->color_table_size = 0;
}

static uint32_t
color_to_uint32 (const pixman_color_t *color)
{
    return
	(color->alpha >> 8 << 24) |
	(color->red >> 8 << 16) |
        (color->green & 0xff00) |
	(color->blue >> 8);
}

static pixman_image_t *
allocate_image (void)
{
    return malloc (sizeof (pixman_image_t *));
}

/* Destructor */
void
pixman_image_destroy (pixman_image_t *image)
{
    
}

/* Constructors */

pixman_image_t *
pixman_image_create_solid_fill (pixman_color_t *color,
				int            *error)
{
    pixman_image_t *img = allocate_image();
    if (!img)
	return NULL;
    
    init_source_image (&img->solid.common);
    
    img->type = SOLID;
    img->solid.color = color_to_uint32 (color);

    return img;
}

pixman_image_t *
pixman_image_create_linear_gradient (pixman_point_fixed_t *p1,
				     pixman_point_fixed_t *p2,
				     int                   n_stops,
				     pixman_fixed_t       *stops,
				     pixman_color_t       *colors,
				     int                  *error)
{
    pixman_image_t *image = allocate_image();
    linear_gradient_t *linear = &image->linear;

    if (!image)
    {
	*error = PIXMAN_BAD_ALLOC;
	return NULL;
    }
    
    if (n_stops < 2)
    {
        *error = PIXMAN_BAD_VALUE;
        return NULL;
    }
    
    init_gradient (&linear->common, n_stops, stops, colors, error);

    linear->p1 = *p1;
    linear->p2 = *p2;

    image->type = LINEAR;

    return image;
}


pixman_image_t *
pixman_image_create_radial_gradient (pixman_point_fixed_t *inner,
				     pixman_point_fixed_t *outer,
				     pixman_fixed_t inner_radius,
				     pixman_fixed_t outer_radius,
				     int             n_stops,
				     pixman_fixed_t *stops,
				     pixman_color_t *colors,
				     int            *error)
{
    pixman_image_t *image = allocate_image();
    radial_gradient_t *radial = &image->radial;

    if (!image)
    {
	*error = PIXMAN_BAD_ALLOC;
	return NULL;
    }
    
    if (n_stops < 2)
    {
        *error = PIXMAN_BAD_VALUE;
	return NULL;
    }

    init_gradient (&radial->common, n_stops, stops, colors, error);

    image->type = RADIAL;
    
    radial->c1.x = inner->x;
    radial->c1.y = inner->y;
    radial->c1.radius = inner_radius;
    radial->c2.x = outer->x;
    radial->c2.y = outer->y;
    radial->c2.radius = outer_radius;
    radial->cdx = (radial->c2.x - radial->c1.x) / 65536.;
    radial->cdy = (radial->c2.y - radial->c1.y) / 65536.;
    radial->dr = (radial->c2.radius - radial->c1.radius) / 65536.;
    radial->A = (  radial->cdx * radial->cdx
		   + radial->cdy * radial->cdy
		   - radial->dr  * radial->dr);

    return image;
}

pixman_image_t *
pixman_image_create_conical_gradient (pixman_point_fixed_t *center,
				      pixman_fixed_t angle,
				      int n_stops,
				      pixman_fixed_t *stops,
				      pixman_color_t *colors,
				      int	     *error)
{
    pixman_image_t *image = allocate_image();
    conical_gradient_t *conical = &image->conical;

    if (!image)
    {
	*error = PIXMAN_BAD_ALLOC;
	return NULL;
    }
    
    if (n_stops < 2)
    {
	*error = PIXMAN_BAD_VALUE;
	return NULL;
    }

    init_gradient (&conical->common, n_stops, stops, colors, error);

    image->type = CONICAL;
    conical->center = *center;
    conical->angle = angle;

    return image;
}

pixman_image_t *
pixman_image_create_bits (pixman_format_code_t    format,
			  int                     width,
			  int                     height,
			  uint32_t	       *bits,
			  int			rowstride,
			  int		       *error)
{
    pixman_image_t *image = allocate_image();

    if (!image)
    {
	*error = PIXMAN_BAD_ALLOC;
	return NULL;
    }
    
    init_common (&image->common);

    if (rowstride & 0x3)
    {
	/* we should probably spew some warning here */
    }
    
    image->type = BITS;
    image->bits.format = format;
    image->bits.width = width;
    image->bits.height = height;
    image->bits.bits = bits;
    image->bits.rowstride = rowstride / 4; /* we store it in number of uint32_t's */
    image->bits.indexed = NULL;

    return image;
}

void
pixman_image_set_clip_region (pixman_image_t    *image,
			      pixman_region16_t *region)
{
    image->common.clip_region = region;
}

void
pixman_image_set_transform         (pixman_image_t       *image,
				    pixman_transform_t   *transform)
{
    image->common.transform = transform;
}

void
pixman_image_set_repeat            (pixman_image_t       *image,
				    pixman_repeat_t       repeat)
{
    image->common.repeat = repeat;
}

void
pixman_image_set_filter            (pixman_image_t       *image,
				    pixman_filter_t       filter)
{
    image->common.filter = filter;
}

void
pixman_image_set_filter_params     (pixman_image_t       *image,
				    pixman_fixed_t       *params,
				    int                   n_params)
{
    image->common.filter_params = params;
    image->common.n_filter_params = n_params;
}

void
pixman_image_set_alpha_map         (pixman_image_t       *image,
				    pixman_image_t       *alpha_map,
				    int16_t               x,
				    int16_t               y)
{
    if (alpha_map && alpha_map->type != BITS)
    {
	image->common.alpha_map = NULL;
	return;
    }
    
    image->common.alpha_map = (bits_image_t *)alpha_map;
    image->common.alpha_origin.x = x;
    image->common.alpha_origin.y = y;
}

void
pixman_image_set_component_alpha   (pixman_image_t       *image,
				    pixman_bool_t         component_alpha)
{
    image->common.component_alpha = component_alpha;
}


#define SCANLINE_BUFFER_LENGTH 2048

void
pixman_image_composite (pixman_op_t	 op,
			pixman_image_t	*src_img,
			pixman_image_t	*mask_img,
			pixman_image_t	*dest_img,
			int		 src_x,
			int		 src_y,
			int		 mask_x,
			int		 mask_y,
			int		 dest_x,
			int		 dest_y,
			int		 width,
			int		 height)
{
    FbComposeData compose_data;
    uint32_t _scanline_buffer[SCANLINE_BUFFER_LENGTH * 3];
    uint32_t *scanline_buffer = _scanline_buffer;

    if (width > SCANLINE_BUFFER_LENGTH)
    {
	scanline_buffer = (uint32_t *)malloc (width * 3 * sizeof (uint32_t));

	if (!scanline_buffer)
	    return;
    }
    
    compose_data.op = op;
    compose_data.src = src_img;
    compose_data.mask = mask_img;
    compose_data.dest = dest_img;
    compose_data.xSrc = src_x;
    compose_data.ySrc = src_y;
    compose_data.xMask = mask_x;
    compose_data.yMask = mask_y;
    compose_data.xDest = dest_x;
    compose_data.yDest = dest_y;
    compose_data.width = width;
    compose_data.height = height;

    fbCompositeRect (&compose_data, scanline_buffer);

    if (scanline_buffer != _scanline_buffer)
	free (scanline_buffer);
}
