

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0361 */
/* at Wed Oct 05 15:26:23 2005
 */
/* Compiler settings for timiditydrv.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __timiditydrv_h__
#define __timiditydrv_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __Itim_synth_FWD_DEFINED__
#define __Itim_synth_FWD_DEFINED__
typedef interface Itim_synth Itim_synth;
#endif 	/* __Itim_synth_FWD_DEFINED__ */


#ifndef __tim_synth_FWD_DEFINED__
#define __tim_synth_FWD_DEFINED__

#ifdef __cplusplus
typedef class tim_synth tim_synth;
#else
typedef struct tim_synth tim_synth;
#endif /* __cplusplus */

#endif 	/* __tim_synth_FWD_DEFINED__ */


#ifdef __cplusplus
extern "C"{
#endif 

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * ); 

#ifndef __Itim_synth_INTERFACE_DEFINED__
#define __Itim_synth_INTERFACE_DEFINED__

/* interface Itim_synth */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_Itim_synth;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D908258C-4B41-41f0-AAD9-684FDAA84C75")
    Itim_synth : public IDispatch
    {
    public:
    };
    
#else 	/* C style interface */

    typedef struct Itim_synthVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            Itim_synth * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            Itim_synth * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            Itim_synth * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            Itim_synth * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            Itim_synth * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            Itim_synth * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            Itim_synth * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        END_INTERFACE
    } Itim_synthVtbl;

    interface Itim_synth
    {
        CONST_VTBL struct Itim_synthVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define Itim_synth_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define Itim_synth_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define Itim_synth_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define Itim_synth_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define Itim_synth_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define Itim_synth_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define Itim_synth_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __Itim_synth_INTERFACE_DEFINED__ */



#ifndef __TIM_DRVLib_LIBRARY_DEFINED__
#define __TIM_DRVLib_LIBRARY_DEFINED__

/* library TIM_DRVLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_TIM_DRVLib;

EXTERN_C const CLSID CLSID_tim_synth;

#ifdef __cplusplus

class DECLSPEC_UUID("0FEC4C35-A705-41d7-A3BB-D587A231045A")
tim_synth;
#endif
#endif /* __TIM_DRVLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


