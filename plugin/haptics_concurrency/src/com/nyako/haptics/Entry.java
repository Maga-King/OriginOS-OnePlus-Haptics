// SPDX-License-Identifier: Apache-2.0
package com.nyako.haptics;

import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.util.Log;
import android.util.SparseArray;
import com.nyako.api.*;
import java.lang.reflect.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

public final class Entry implements NyakoModule {
    private static final String TAG="NyakoHaptics";
    private static final AtomicLong sequence=new AtomicLong();
    private static final Map<IBinder,Owner> owners=new WeakHashMap<>();
    private static volatile IBinder extension;
    private static final class Owner {
        final long id; final int usage;
        Owner(long id,int usage){this.id=id;this.usage=usage;}
    }
    private static Object field(Object object,String name)throws Exception {
        for(Class<?> c=object.getClass();c!=null;c=c.getSuperclass()){
            try{Field f=c.getDeclaredField(name);f.setAccessible(true);return f.get(object);}
            catch(NoSuchFieldException ignored){}
        }
        throw new NoSuchFieldException(name);
    }
    private static Object invoke(Object object,String name)throws Exception {
        for(Class<?> c=object.getClass();c!=null;c=c.getSuperclass()){
            try{Method m=c.getDeclaredMethod(name);m.setAccessible(true);return m.invoke(object);}
            catch(NoSuchMethodException ignored){}
        }
        throw new NoSuchMethodException(name);
    }
    private static IBinder hal()throws Exception {
        IBinder cached=extension;if(cached!=null&&cached.isBinderAlive())return cached;
        Class<?> manager=Class.forName("android.os.ServiceManager");
        Method check=manager.getDeclaredMethod("checkService",String.class);check.setAccessible(true);
        IBinder base=(IBinder)check.invoke(null,"android.hardware.vibrator.IVibrator/default");
        if(base==null)return null;
        Method get=IBinder.class.getDeclaredMethod("getExtension");get.setAccessible(true);
        extension=(IBinder)get.invoke(base);return extension;
    }
    private static int send(int effect,int strength,long owner)throws Exception {
        long identity=Binder.clearCallingIdentity();
        Parcel in=Parcel.obtain(),out=Parcel.obtain();
        try{
            IBinder binder=hal();if(binder==null)return -19;
            in.writeInterfaceToken("vendor.aac.hardware.richtap.vibrator.IRichtapVibrator");
            in.writeInt(effect);in.writeInt(strength);in.writeLong(owner);
            if(!binder.transact(10001,in,out,0))return -95;
            out.readException();return out.readInt();
        }finally{in.recycle();out.recycle();Binder.restoreCallingIdentity(identity);}
    }
    private static void cancel(long owner)throws Exception {
        long identity=Binder.clearCallingIdentity();
        Parcel in=Parcel.obtain(),out=Parcel.obtain();
        try{
            IBinder binder=hal();if(binder==null)return;
            in.writeInterfaceToken("vendor.aac.hardware.richtap.vibrator.IRichtapVibrator");in.writeLong(owner);
            if(binder.transact(10002,in,out,0))out.readException();
        }finally{in.recycle();out.recycle();Binder.restoreCallingIdentity(identity);}
    }
    private static void supplement(HookCall call)throws Exception {
        if(call.hasThrowable()||call.getResult()==null)return;
        if(!"IGNORED_FOR_HIGHER_IMPORTANCE".equals(String.valueOf(field(call.getResult(),"status"))))return;
        Object session=call.arguments[0];
        if(!session.getClass().getName().endsWith(".SingleVibrationSession"))return;
        if((Boolean)invoke(session,"isRepeating"))return;
        Object current=field(call.receiver,"mCurrentSession");if(current==null)return;
        // Keep vendor/external ownership under the original framework. Only
        // ordinary sessions backed by our normal PCM lane can be mixed.
        if(!current.getClass().getName().endsWith(".SingleVibrationSession"))return;
        if((Boolean)invoke(current,"wasEndRequested"))return;
        Object currentAttrs=field(invoke(current,"getCallerInfo"),"attrs");
        int currentUsage=(Integer)invoke(currentAttrs,"getUsage");
        Object caller=invoke(session,"getCallerInfo"),attrs=field(caller,"attrs");
        int usage=(Integer)invoke(attrs,"getUsage");
        if(usage!=18&&usage!=50&&usage!=82&&usage!=98)return;
        Object combined=invoke(invoke(session,"getVibration"),"getEffectToPlay");
        Object effect;
        if(combined.getClass().getName().equals("android.os.CombinedVibration$Mono"))effect=invoke(combined,"getEffect");
        else if(combined.getClass().getName().equals("android.os.CombinedVibration$Stereo")){
            SparseArray<?> effects=(SparseArray<?>)invoke(combined,"getEffects");
            if(effects.size()!=1)return;effect=effects.valueAt(0);
        }else return;
        if((Integer)invoke(effect,"getRepeatIndex")!=-1)return;
        List<?> segments=(List<?>)invoke(effect,"getSegments");
        if(segments.size()!=1){Log.d(TAG,"unsupported short composition segments="+segments.size());return;}
        Object segment=segments.get(0);String type=segment.getClass().getName();
        if(!type.equals("android.os.vibrator.PrebakedSegment")&&!type.equals("android.os.vibrator.ExtPrebakedSegment")){
            Log.d(TAG,"unsupported short segment="+type);return;
        }
        int id=(Integer)invoke(segment,"getEffectId"),strength=(Integer)invoke(segment,"getEffectStrength");
        if(type.endsWith("ExtPrebakedSegment")){
            int stepless=(Integer)invoke(segment,"getStepless");if(stepless!=0)strength=stepless;
        }
        IBinder token=(IBinder)invoke(session,"getCallerToken");
        long owner=sequence.incrementAndGet();
        int result=send(id,strength,owner);
        if(result>0){synchronized(owners){owners.put(token,new Owner(owner,usage));}}
        Log.i(TAG,"parallel effect="+id+" usage="+usage+" currentUsage="+currentUsage+
            " package="+field(caller,"opPkg")+" owner="+owner+" result="+result);
        // Preserve the original EndInfo. Framework accounting remains honest:
        // the original session was rejected; this is a separate HAL feedback.
    }
    @Override public void onPackageReady(ModuleContext context)throws Throwable {
        if(!ModuleConfig.bool(context.config.snapshot(),"parallel_feedback",true))return;
        Class<?> service=Class.forName("com.android.server.vibrator.VibratorManagerService",false,context.classLoader);
        Class<?> session=Class.forName("com.android.server.vibrator.VibrationSession",false,context.classLoader);
        Method priority=service.getDeclaredMethod("shouldIgnoreForOngoingLocked",session);priority.setAccessible(true);
        context.hookOnce("ongoing-short-feedback",priority,0,new HookCallback(){
            @Override public void after(HookCall call){
                try{supplement(call);}catch(Exception e){Log.w(TAG,"parallel dispatch failed",e);}
            }
        });
        Method cancel=service.getDeclaredMethod("cancelVibrateInternal",int.class,IBinder.class);cancel.setAccessible(true);
        context.hookOnce("cancel-short-owner",cancel,0,new HookCallback(){
            @Override public void after(HookCall call){
                if(call.hasThrowable())return;
                List<Owner> cancelled=new ArrayList<>();int filter=(Integer)call.arguments[0];
                IBinder token=(IBinder)call.arguments[1];
                synchronized(owners){
                    Iterator<Map.Entry<IBinder,Owner>> it=owners.entrySet().iterator();
                    while(it.hasNext()){
                        Map.Entry<IBinder,Owner> entry=it.next();Owner owner=entry.getValue();
                        if((token==null||token==entry.getKey())&&(filter&owner.usage)==owner.usage){
                            cancelled.add(owner);it.remove();
                        }
                    }
                }
                for(Owner owner:cancelled){
                    try{Entry.cancel(owner.id);}catch(Exception e){Log.w(TAG,"parallel cancel failed",e);}
                }
            }
        });
        Log.i(TAG,"installed ongoing-session feedback and owner cancellation hooks");
    }
}
