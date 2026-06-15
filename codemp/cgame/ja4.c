	
/*	if (!thirdPerson) {
		refEntity_t ent;
		vec3_t ang;
		float scale;
		memset( &ent, 0, sizeof( ent ) );
		VectorCopy( flash.origin, ent.origin );
		AxisToAngles(flash.axis, ang);
		ang[2] = 0.0f;
		AnglesToAxis(ang, ent.axis);
		ent.radius = 7.0f;
		switch(cent->weapon) {
		case WP_THERMAL:
			scale = 13.37f;
			break;
		case WP_ROCKET_LAUNCHER:
			scale = 5.0f;
			break;
		case WP_FLECHETTE:
			ent.radius = 8.17f;
			scale = 0.0f;
			break;
		default:
			scale = 5.0f;
			break;
		}
		VectorMA( ent.origin, scale, ent.axis[0], ent.origin );
		VectorMA( ent.origin, -6.4, cg.refdef.viewaxis[1], ent.origin );
		VectorMA( ent.origin, -4.2f, ent.axis[2], ent.origin );
		
		ent.reType = RT_ORIENTED_QUAD;
		ent.customShader = trap_R_RegisterShaderNoMip("gfx/hud/player");
		
		MAKERGBA( ent.shaderRGBA, 0, 0, 255, 200 );
		ent.renderfx = RF_FORCE_ENT_ALPHA | RF_DEPTHHACK | RF_FIRST_PERSON;
		
//		AnglesToAxis( angles, ent.axis );
/*		VectorCopy( flash.axis[0], ent.axis[0] );
		VectorCopy( flash.axis[1], ent.axis[1] );
		VectorCopy( flash.axis[2], ent.axis[2] );*/

		// render it, flip it around, render it again
/*		trap_R_AddRefEntityToScene( &ent );
	}*/

