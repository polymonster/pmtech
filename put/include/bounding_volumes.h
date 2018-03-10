#ifndef _BOUNDING_VOLUMES_H
#define _BOUNDING_VOLUMES_H

#if 0

#include "vector.h"
#include "matrix.h"

namespace psengine
{
	/*=========================================================*\
	|	OBB3D - 3D Oriented Bounding Box
	\*=========================================================*/
	class OBB3D
	{
		public:

			OBB3D();
			OBB3D(vec3f position, vec3f size, vec3f rotation);
			OBB3D(mat4 m_orientation_matrix);

			mat4 m_orientation_matrix;

			vec3f m_position;
			vec3f m_size;
			vec3f m_rotation;

			bool m_under_collision;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();
			void build_matrix();

		private:

			vec3f m_widget_selected_axis;
	};

	/*=========================================================*\
	|	Convex Hull - 3D Convex shape
	\*=========================================================*/
#if 0
	class CONVEX_HULL
	{
		public:

			CONVEX_HULL(){};
			CONVEX_HULL(vec3f position, vec3f size, vec3f rotation, Vector3fArray vertices, IndexArray indices);

			mat4 m_orientation_matrix;
			mat4 m_rotation_matrix;

			vec3f m_position;
			vec3f m_size;
			vec3f m_rotation;

			Vector3fArray m_vertices;
			Vector3fArray m_axes;

			IndexArray m_indices;

			bool m_under_collision;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();
			void build_matrix();

		private:

			vec3f m_widget_selected_axis;
	};
#endif

	/*=========================================================*\
	|	AABB3D - 3D Axially Aligned Bounding Box
	\*=========================================================*/
	class AABB3D
	{
		public:

			AABB3D();
			AABB3D(vec3f position, vec3f size);

			bool m_under_collision;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();

			vec3f m_position;
			vec3f m_size;

		private:

			vec3f m_widget_selected_axis;

	};

	/*=========================================================*\
	|	SPHERE - 3D Sphere
	\*=========================================================*/
	class SPHERE
	{
		public:

			SPHERE();
			SPHERE(vec3f position, f32 radius );

			vec3f m_position;
			f32 m_radius;
			bool m_under_collision;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();

		private:

			vec3f m_widget_selected_axis;

	};

	/*=========================================================*\
	|	CIRCLE - 2D Circle
	\*=========================================================*/
	class CIRCLE
	{
		public:

			CIRCLE();
			CIRCLE(vec3f position, f32 radius );

			vec3f m_position;
			f32 m_radius;
			bool m_under_collision;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();

		private:

			vec3f m_widget_selected_axis;

	};

	/*=========================================================*\
	|	AA_ELLIPSOID - 3D Axially Aligned Ellipsoid
	\*=========================================================*/
	class AA_ELLIPSOID
	{
		public:

			AA_ELLIPSOID();
			AA_ELLIPSOID(vec3f position, vec3f radii );

			vec3f m_position;
			vec3f m_radii;
			bool m_under_collision;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();

		private:

			vec3f m_widget_selected_axis;

	};

	/*=========================================================*\
	|	TRIANGLE
	\*=========================================================*/
	class TRIANGLE
	{
		public:

			TRIANGLE();
			TRIANGLE(Vector3fArray vertices);

			vec3f m_vertices[3];

			bool m_under_collision;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();

		private:

			vec3f m_widget_selected_axis;

	};

	/*=========================================================*\
	|	LINE_2D - 3D Line
	\*=========================================================*/
	class LINE_3D
	{
		public:

			LINE_3D();
			LINE_3D(vec3f line_start, vec3f line_end);

			vec3f m_line_start;
			vec3f m_line_end;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();

		private:

			vec3f m_widget_selected_axis;
	};

	/*=========================================================*\
	|	LINE_2D - 2D Line
	\*=========================================================*/
	class LINE_2D
	{
		public:

			LINE_2D();
			LINE_2D(vec2f line_start, vec2f line_end);

			vec2f m_line_start;
			vec2f m_line_end;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();

		private:

			vec3f m_widget_selected_axis;
	};

	/*=========================================================*\
	|	RAY_3D - 3D Ray
	\*=========================================================*/
	class RAY_3D
	{
		public:

			RAY_3D(){};
			RAY_3D(vec3f pos32_on_ray, vec3f direction_vector);

			vec3f m_pos32_on_ray;
			vec3f m_direction_vector;
	};

	/*=========================================================*\
	|	RAY_2D - 2D Ray
	\*=========================================================*/
	class RAY_2D
	{
		public:

			RAY_2D();
			RAY_2D(vec2f pos32_on_ray, vec2f direction_vector);

			vec2f m_pos32_on_ray;
			vec2f m_direction_vector;

			void update();
			void render();
			void aux_render();
			void back_buffer_render();

		private:

			vec3f m_widget_selected_axis;
	};

	class PLANE
	{
	public:

		PLANE(){};
		PLANE(vec3f pos32_on_plane, vec3f normal);

		vec3f m_pos32_on_plane;
		vec3f m_normal;
	};

}

/*================== EXTERNAL VARIABLES ===================*/

#endif //_BOUNDING_VOLUMES_H

#endif
